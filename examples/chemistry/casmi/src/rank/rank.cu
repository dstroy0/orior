// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank.cu: casmi_driver --rank. Each query molecule's train structures, ranked by their reference spectra against the
// molecule's own, read from a set the ingest sealed. Every value is an exact integer on the record machine and every
// verdict a sign the device returned; the host moves record numbers by those signs and does nothing else to a value.
//
//   ion:     I = 10 x sum (molecules x f_k + a_k) x M_k - charge x e, every candidate formula with every query adduct,
//            in 10^-16 Da, the CODATA 2022 electron
//   window:  the query's precursor x = n / d, d a power of ten or of two: D = n |z| 10^16 - I d and N = I d; the
//            formula is inside where D / N is inside the envelope of the query's mode, by two COMPARE signs
//   match:   every query peak q against every peak r of every reference spectrum the window names, each m/z n / d:
//            e = n_q d_r - n_r d_q against n_r d_q by the same two signs, and w = (1 - c_high)(1 + c_low) I_q I_r, each
//            intensity on its row's one scale
//   peak:    each query peak's heaviest w by a COMPARE tournament
//   sum:     M = the sum of a pair's kept w; Q and R = the sums of I^2 over each spectrum's peaks
//   score:   (M^2, Q R), put in order by COMPARE(M_a^2 Q_b R_b, M_b^2 Q_a R_a)
//   list:    a molecule's structures, the scored by repeated tournaments heaviest first, then the unscored in
//            structure order, the first 25 written
#include "rank.h"
#include "rank_internal.h"

#include "../../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../mass/mass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#define RANK_LIST_MOST 25u
#define RANK_MODES 2u
#define RANK_ENVELOPE_FIELDS 4u
#define RANK_ENVELOPE_LIMBS 8u
#define RANK_LOW_D 0u
#define RANK_LOW_N 1u
#define RANK_HIGH_D 2u
#define RANK_HIGH_N 3u
#define RANK_TOKENS_MOST 1024u
#define RANK_PATH_BYTES 1024u
#define RANK_TERMS_MOST (2u * CASMI_FORMULA_ELEMENTS_MOST)
#define RANK_ADDUCTS_MOST 64u

// 10^-16 Da in one dalton, the ion's unit
#define RANK_PER_DALTON 10000000000000000ull

// the device bytes the job declares: the widest sweep's records, members and index
#define RANK_DECLARED (2ull << 30u)

// lanes a sweep runs at once, the most a chunk of match lanes holds
#define RANK_CHUNK_LANES (1ull << 22u)

// a double's fields as IEEE 754 binary64 lays them, and the exponent its integer mantissa stands at: x = M 2^(E - 1075)
#define RANK_MANTISSA_BITS 52u
#define RANK_EXPONENT_MASK 0x7FFull
#define RANK_UNIT_EXPONENT 1075u

// A rational's code beside its numerator: the decimal places of a power-of-ten denominator, or past
// RANK_CODE_KEPT the stored exponent E of a power-of-two one, 2^(1075 - E)
#define RANK_CODE_KEPT 2048u
#define RANK_CODE_BITS 12u

// an m/z's denominator: a kept one's 2^(1075 - E) with E at least 1023, the m/z at least 1, below 2^6; a decimal's
// 10^places with places at most 12, below 2^4
#define RANK_TWO_EXPONENT_BITS 6u
#define RANK_TEN_EXPONENT_BITS 4u

// the precursor's and the peaks' unit places where a row's values take each their own form
#define RANK_UNIT_PLACES_MZ 4u
#define RANK_UNIT_PLACES_INTENSITY 6u

static const unsigned long long RANK_DIVISOR[RANK_DIVISORS] = {100ull, 999ull};

typedef struct
{
    AnchorExactInteger part[RANK_ENVELOPE_FIELDS];
} RankEnvelope;

// one isotope of an ion: the formula's and the adduct's counts of it
typedef struct
{
    unsigned int isotope;
    long long formula;
    long long adduct;
} RankTerm;

// groups of record numbers: group g holds entry[first[g] .. first[g] + count[g])
typedef struct
{
    const unsigned long long *first;
    const unsigned long long *count;
    const unsigned int *entry;
    unsigned long long group_count;
} RankGroups;

// ---- reading ----

static int rank_hex_read(const char *text, AnchorExactInteger *value)
{
    anchor_exact_zero(value);
    const char *at = text;
    const int negative = (*at == '-');
    at += negative ? 1 : 0;
    if ((at[0] != '0') || (at[1] != 'x'))
    {
        return 0;
    }
    at += 2;
    const size_t digits = strlen(at);
    if ((digits == 0u) || (digits > (RANK_ENVELOPE_LIMBS * 8u)))
    {
        return 0;
    }
    for (size_t digit = 0u; digit < digits; digit += 1u)
    {
        const char held = at[digits - 1u - digit];
        const int nibble = ((held >= '0') && (held <= '9'))   ? (held - '0')
                           : ((held >= 'a') && (held <= 'f')) ? (held - 'a' + 10)
                           : ((held >= 'A') && (held <= 'F')) ? (held - 'A' + 10)
                                                              : -1;
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

// a text's id in the set's table of that column, RANK_ABSENT where the set holds no such string
static unsigned int rank_string_find(const RankSet *set, unsigned int text, const char *wanted)
{
    for (size_t each = 0u; each < set->strings[text].size(); each += 1u)
    {
        if (set->strings[text][each] == wanted)
        {
            return (unsigned int)each;
        }
    }
    return RANK_ABSENT;
}

static unsigned int rank_terms_merge(const CasmiFormula *formula, const CasmiAdduct *adduct, RankTerm *term)
{
    unsigned int count = 0u;
    for (unsigned int slot = 0u; slot < (formula->element_count + adduct->terms.element_count); slot += 1u)
    {
        const int from_formula = slot < formula->element_count;
        const unsigned int at = from_formula ? slot : (slot - formula->element_count);
        const unsigned int named = from_formula ? formula->isotope[at] : adduct->terms.isotope[at];
        const long long held = from_formula ? formula->count[at] : adduct->terms.count[at];
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
        term[entry].formula += from_formula ? held : 0ll;
        term[entry].adduct += from_formula ? 0ll : held;
    }
    return count;
}

static unsigned int rank_magnitude_bits(long long value)
{
    return exact_record_bits_of((value < 0ll) ? (unsigned long long)(-value) : (unsigned long long)value);
}

static void rank_exact_signed(long long value, AnchorExactInteger *out)
{
    rank_exact_of((value < 0ll) ? (unsigned long long)(-value) : (unsigned long long)value, out);
    out->sign = (value < 0ll) ? -out->sign : out->sign;
}

// ---- a value as a rational n / d ----

// A column's value read as its numerator and its code: the integer on its row's unit and the row's places where the
// row holds one form, the column's unit places where it does not, and a kept value's mantissa and exponent
static void rank_rational(const RankColumn *column, unsigned long long row, unsigned long long value,
                          unsigned int unit_places, unsigned long long *numerator, unsigned int *code)
{
    const unsigned int term = column->term[row];
    if (column->form[value] == RANK_FORM_KEPT)
    {
        const unsigned long long stored = column->unit[value];
        const unsigned long long biased = (stored >> RANK_MANTISSA_BITS) & RANK_EXPONENT_MASK;
        const unsigned long long fraction = stored & ((1ull << RANK_MANTISSA_BITS) - 1ull);
        *numerator = fraction | ((biased != 0ull) ? (1ull << RANK_MANTISSA_BITS) : 0ull);
        *code = RANK_CODE_KEPT + (unsigned int)((biased != 0ull) ? biased : 1ull);
        return;
    }
    *numerator = column->unit[value];
    *code = ((term % RANK_TERM_FORMS) == RANK_ROW_EACH) ? unit_places : (term / RANK_TERM_FORMS);
}

// a code's denominator: 10^places, or 2^(1075 - E) for a kept value
static void rank_denominator(unsigned int code, AnchorExactInteger *out)
{
    rank_exact_of(1ull, out);
    AnchorExactInteger factor;
    const int kept = code >= RANK_CODE_KEPT;
    const unsigned int times = kept ? (RANK_UNIT_EXPONENT - (code - RANK_CODE_KEPT)) : code;
    rank_exact_of(kept ? 2ull : 10ull, &factor);
    for (unsigned int each = 0u; each < times; each += 1u)
    {
        (void)anchor_exact_multiply(out, &factor, out);
    }
}

// An intensity row on one scale: each value's integer n, n / L its value, L the least common multiple of the row's
// denominators. A row of one form takes its integers as they are, the row's one denominator canceling in every score;
// a row of no one form has each value's denominator, B a count's, 10^6 a decimal's, the divisor times 10^6 a divided
// decimal's and 2^(1075 - E) a kept value's, and its integer times L over it
static int rank_intensity_row(const RankSet *set, unsigned long long row, std::vector<AnchorExactInteger> *out)
{
    const RankColumn *const column = &set->intensity;
    const unsigned long long first = column->row_start[row];
    const unsigned long long past = column->row_start[row + 1ull];
    out->assign((size_t)(past - first), AnchorExactInteger());
    const int each = (column->term[row] % RANK_TERM_FORMS) == RANK_ROW_EACH;
    if (!each)
    {
        for (unsigned long long value = first; value < past; value += 1ull)
        {
            rank_exact_of(column->unit[value], &(*out)[value - first]);
        }
        return 1;
    }
    std::vector<AnchorExactInteger> numerator((size_t)(past - first));
    std::vector<AnchorExactInteger> denominator((size_t)(past - first));
    AnchorExactInteger common;
    rank_exact_of(1ull, &common);
    int ok = 1;
    for (unsigned long long value = first; ok && (value < past); value += 1ull)
    {
        AnchorExactInteger *const n = &numerator[value - first];
        AnchorExactInteger *const d = &denominator[value - first];
        const unsigned int form = column->form[value];
        if (form == RANK_FORM_COUNT)
        {
            // the count's base, a whole number the ingest held it as
            double base = 0.0;
            memcpy(&base, &set->base[row], 8u);
            ok = set->base_held[row] && (base >= 1.0) && (base < 9007199254740992.0);
            rank_exact_of(column->unit[value], n);
            rank_exact_of(ok ? (unsigned long long)base : 1ull, d);
        }
        else if (form == RANK_FORM_KEPT)
        {
            unsigned long long mantissa = 0ull;
            unsigned int code = 0u;
            rank_rational(column, row, value, RANK_UNIT_PLACES_INTENSITY, &mantissa, &code);
            ok = (code - RANK_CODE_KEPT) <= RANK_UNIT_EXPONENT;
            rank_exact_of(mantissa, n);
            rank_denominator(ok ? code : 0u, d);
        }
        else
        {
            rank_exact_of(column->unit[value], n);
            rank_denominator(RANK_UNIT_PLACES_INTENSITY, d);
            if (form != RANK_FORM_DECIMAL)
            {
                AnchorExactInteger divisor;
                rank_exact_of(RANK_DIVISOR[form - RANK_FORM_DIVIDED], &divisor);
                (void)anchor_exact_multiply(d, &divisor, d);
            }
        }
        // the least common multiple: common d / gcd(common, d)
        AnchorExactInteger divides;
        AnchorExactInteger part;
        ok = ok && (anchor_exact_gcd(&common, d, &divides) == ANCHOR_EXACT_OK) &&
             (anchor_exact_divide_exact(d, &divides, &part) == ANCHOR_EXACT_OK) &&
             (anchor_exact_multiply(&common, &part, &common) == ANCHOR_EXACT_OK);
    }
    for (unsigned long long value = first; ok && (value < past); value += 1ull)
    {
        AnchorExactInteger lift;
        ok = (anchor_exact_divide_exact(&common, &denominator[value - first], &lift) == ANCHOR_EXACT_OK) &&
             (anchor_exact_multiply(&numerator[value - first], &lift, &(*out)[value - first]) == ANCHOR_EXACT_OK);
    }
    return ok;
}

// ---- programs ----

// sign(a - b) as a register: 1, 0 or -1
static unsigned int rank_compare(ExactRecordProgram *program, unsigned int a, unsigned int b)
{
    return exact_record_difference(program, exact_record_above(program, a, b), exact_record_above(program, b, a));
}

// a rational's denominator from its code: 10^places, or 2^(1075 - E) past RANK_CODE_KEPT
static unsigned int rank_denominator_steps(ExactRecordProgram *program, unsigned int code)
{
    const unsigned int kept_at = exact_record_constant(program, RANK_CODE_KEPT);
    const unsigned int kept = exact_record_quotient(program, code, kept_at);
    const unsigned int low = exact_record_remainder(program, code, kept_at);
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int two_exponent =
        exact_record_select(program, kept, exact_record_difference(program, exact_record_constant(program, RANK_UNIT_EXPONENT), low),
                            zero);
    const unsigned int ten_exponent = exact_record_select(program, kept, zero, low);
    return exact_record_product(program, exact_record_two_to(program, two_exponent, RANK_TWO_EXPONENT_BITS),
                                exact_record_power_of(program, 10ull, ten_exponent, RANK_TEN_EXPONENT_BITS));
}

// The envelope's two signs for the ratio e / n, n > 0, the envelope's fields read from member `member`:
// c_high = COMPARE(2 e N_high, 2 D_high n + 1) and c_low = COMPARE(2 e N_low, 2 D_low n - 1). Neither side of either is
// ever equal to the other; each sign is -1 or +1, and e / n is inside [D_low / N_low, D_high / N_high] exactly where
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

// where a value record holds a rational: its numerator and its code
typedef struct
{
    unsigned int numerator_bits;
    unsigned int code_offset;
    unsigned int intensity_offset;
    unsigned int intensity_bits;
    unsigned int limbs;
} RankPeakLayout;

// ---- groups on the device ----

// The sum program: three records' one field, summed
static int rank_sum_lay(SimResults *results, RankField value, unsigned int limbs, RankMachine *machine)
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

// Sums each group's records on the device, three to a lane, pass after pass, until each group is one record; every
// group takes every pass, a zero pad filling a short lane; every sum ends in one layout. *summed holds group g's
// sum at g, then the pad
static int rank_group_sum(SimResults *results, const char *name, RankField value, unsigned int value_limbs,
                          const unsigned int *records, unsigned long long record_count, const RankGroups *groups,
                          std::vector<unsigned int> *summed, RankField *summed_field, unsigned int *summed_limbs,
                          unsigned long long *microseconds)
{
    unsigned long long entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        entry_total += groups->count[group];
    }
    unsigned long long bodies = record_count + 1ull;
    std::vector<unsigned int> current((size_t)(bodies * value_limbs), 0u);
    memcpy(current.data(), records, (size_t)(record_count * value_limbs * sizeof(unsigned int)));
    std::vector<unsigned int> entry((size_t)entry_total + 1u);
    std::vector<unsigned long long> first((size_t)groups->group_count + 1u);
    std::vector<unsigned long long> count((size_t)groups->group_count + 1u);
    unsigned long long widest = 0ull;
    entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        first[group] = entry_total;
        count[group] = groups->count[group];
        memcpy(&entry[entry_total], &groups->entry[groups->first[group]], (size_t)(count[group] * sizeof(unsigned int)));
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
        if (!rank_sum_lay(results, field, limbs, &sum))
        {
            return 0;
        }
        std::vector<unsigned int> index((size_t)(3ull * lanes) + 1u);
        const unsigned int pad = (unsigned int)(bodies - 1ull);
        unsigned long long lane = 0ull;
        widest = 0ull;
        for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
        {
            const unsigned long long counted = (count[group] + 2ull) / 3ull;
            const unsigned long long group_lanes = (counted == 0ull) ? 1ull : counted;
            for (unsigned long long each = 0ull; each < group_lanes; each += 1ull)
            {
                for (unsigned int member = 0u; member < 3u; member += 1u)
                {
                    const unsigned long long at = (3ull * each) + member;
                    index[(3ull * (lane + each)) + member] = (at < count[group]) ? entry[first[group] + at] : pad;
                }
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
        std::vector<unsigned int> next((size_t)((lanes + 1ull) * out_limbs), 0u);
        const unsigned int *const members[ENGINE_RECORD_MEMBERS_MAX] = {current.data(), current.data(), current.data()};
        const unsigned long long member_bodies[ENGINE_RECORD_MEMBERS_MAX] = {bodies, bodies, bodies};
        if (!rank_sweep(results, name, &sum, members, member_bodies, index.data(), lanes, next.data(), microseconds))
        {
            rank_machine_release(&sum);
            return 0;
        }
        field = rank_output(&sum, 0u);
        limbs = out_limbs;
        rank_machine_release(&sum);
        current.swap(next);
        bodies = lanes + 1ull;
        passes += 1u;
    }
    summed->swap(current);
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
    std::vector<unsigned int> entry((size_t)entry_total + 1u);
    std::vector<unsigned long long> first((size_t)groups->group_count + 1u);
    std::vector<unsigned long long> count((size_t)groups->group_count + 1u);
    std::vector<unsigned int> index((size_t)entry_total + 2u);
    const unsigned int out_limbs = order->layout.out_limbs;
    std::vector<unsigned int> signs((size_t)((entry_total / 2ull) + 1ull) * out_limbs);
    entry_total = 0ull;
    for (unsigned long long group = 0ull; group < groups->group_count; group += 1ull)
    {
        first[group] = entry_total;
        count[group] = groups->count[group];
        memcpy(&entry[entry_total], &groups->entry[groups->first[group]], (size_t)(count[group] * sizeof(unsigned int)));
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
        if (!rank_sweep(results, name, order, members, member_bodies, index.data(), lanes, signs.data(), microseconds))
        {
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
    return 1;
}

// the value order program: COMPARE(v_a, v_b), one field read from two records
static int rank_value_order_lay(SimResults *results, RankField value, unsigned int limbs, RankMachine *machine)
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
static int rank_ratio_order_lay(SimResults *results, RankField numerator, RankField denominator, unsigned int limbs,
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

static void rank_tally_line(SimResults *results, const char *name, const RankTally *tally)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, name);
    rank_line_decimal(results, ": ", tally->lanes);
    rank_line_decimal(results, " lanes, device ", tally->microseconds);
    scriptura_text(&results->line, " us");
    rank_line_end(results);
}

int rank_run(SimResults *results, int count, char **arguments)
{
    const char *const set_path = arguments[2];
    const char *const cfg_path = arguments[3];
    const int validate = strcmp(arguments[4], "validate") == 0;
    if (!validate)
    {
        scriptura_text(&results->line, "  --rank runs validate; test ranks a test set this build does not read yet");
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
    static CfgJsonToken tokens[RANK_TOKENS_MOST];
    CfgJsonParse parse;
    if ((cfg_length == 0u) || !cfg_json_parse(cfg_text, cfg_length, tokens, RANK_TOKENS_MOST, &parse))
    {
        scriptura_text(&results->line, "  the cfg did not read: ");
        scriptura_text(&results->line, cfg_path);
        rank_line_end(results);
        return 0;
    }
    static const char *const mode_name[RANK_MODES] = {"positive", "negative"};
    static const char *const envelope_name[RANK_ENVELOPE_FIELDS] = {"low_D", "low_N", "high_D", "high_N"};
    const unsigned int envelope_block = cfg_json_member(cfg_text, tokens, 0u, "envelope");
    const unsigned int validate_block = cfg_json_member(cfg_text, tokens, 0u, "validate");
    char query_library[RANK_PATH_BYTES];
    int ok = rank_cfg_string(cfg_text, tokens, validate_block, "query_library", query_library);
    RankEnvelope envelope[RANK_MODES];
    RankEnvelopeLayout envelope_layout;
    memset(&envelope_layout, 0, sizeof(envelope_layout));
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
        scriptura_text(&results->line, "  the cfg lacks validate.query_library or an envelope with a positive N");
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
    std::vector<unsigned int> envelope_records((size_t)(RANK_MODES * envelope_layout.limbs), 0u);
    for (unsigned int mode = 0u; mode < RANK_MODES; mode += 1u)
    {
        for (unsigned int part = 0u; part < RANK_ENVELOPE_FIELDS; part += 1u)
        {
            rank_field_put(&envelope_records[mode * envelope_layout.limbs], envelope_layout.offset[part],
                           envelope_layout.bits[part], &envelope[mode].part[part]);
        }
    }

    // the set
    static RankSet set;
    if (!rank_set_load(results, set_path, &set))
    {
        scriptura_text(&results->line, "  the set did not read: ");
        scriptura_text(&results->line, set_path);
        rank_line_end(results);
        return 0;
    }
    const unsigned long long spectra = set.spectra;
    rank_line_decimal(results, "  set: ", spectra);
    rank_line_decimal(results, " spectra in ", (unsigned long long)set.row_groups);
    rank_line_decimal(results, " row groups, ", set.mz.unit.size());
    rank_line_decimal(results, " peaks, ", (unsigned long long)set.strings[RANK_KEY].size());
    scriptura_text(&results->line, " structures");
    rank_line_end(results);
    if (sim_job_submit(results, "casmi_driver", count, arguments, RANK_DECLARED) == 0)
    {
        return 0;
    }

    // each spectrum's mode, the held-out library, and the reference spectra of each structure
    unsigned int mode_of_string[2] = {rank_string_find(&set, RANK_MODE, mode_name[0]),
                                      rank_string_find(&set, RANK_MODE, mode_name[1])};
    std::vector<unsigned int> mode((size_t)spectra, RANK_ABSENT);
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int named = set.text[RANK_MODE][spectrum];
        mode[spectrum] = (named == mode_of_string[0]) ? 0u : (named == mode_of_string[1]) ? 1u : RANK_ABSENT;
    }
    const unsigned int held_out = rank_string_find(&set, RANK_LIBRARY, query_library);
    if (held_out == RANK_ABSENT)
    {
        scriptura_text(&results->line, "  validate.query_library is no library of the set: ");
        scriptura_text(&results->line, query_library);
        rank_line_end(results);
        return 0;
    }
    const unsigned long long structures = set.strings[RANK_KEY].size();
    std::vector<unsigned char> reference((size_t)spectra, 0u);
    std::vector<unsigned long long> structure_reference_first((size_t)structures + 1u, 0ull);
    unsigned long long reference_count = 0ull;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int structure = set.text[RANK_KEY][spectrum];
        reference[spectrum] = (unsigned char)((mode[spectrum] != RANK_ABSENT) && (structure != RANK_ABSENT) &&
                                              (set.text[RANK_LIBRARY][spectrum] != held_out));
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
    std::vector<unsigned int> structure_reference((size_t)reference_count + 1u);
    {
        std::vector<unsigned long long> filled((size_t)structures + 1u, 0ull);
        for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
        {
            if (reference[spectrum])
            {
                const unsigned int structure = set.text[RANK_KEY][spectrum];
                structure_reference[structure_reference_first[structure] + filled[structure]] = (unsigned int)spectrum;
                filled[structure] += 1ull;
            }
        }
    }
    rank_line_decimal(results, "  reference spectra: ", reference_count);
    rank_line_end(results);

    // the queries and their molecules: the held-out library's spectra of a known mode, grouped by structure
    std::vector<unsigned int> query;
    std::vector<unsigned int> molecule_of_key((size_t)structures + 1u, RANK_ABSENT);
    std::vector<unsigned int> molecule_key;
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int structure = set.text[RANK_KEY][spectrum];
        if ((set.text[RANK_LIBRARY][spectrum] != held_out) || (mode[spectrum] == RANK_ABSENT) ||
            (structure == RANK_ABSENT))
        {
            continue;
        }
        if (molecule_of_key[structure] == RANK_ABSENT)
        {
            molecule_of_key[structure] = (unsigned int)molecule_key.size();
            molecule_key.push_back(structure);
        }
        query.push_back((unsigned int)spectrum);
    }
    const unsigned long long query_count = query.size();
    const unsigned long long molecule_count = molecule_key.size();
    std::vector<unsigned long long> molecule_query_first((size_t)molecule_count + 2u, 0ull);
    std::vector<unsigned int> molecule_query((size_t)query_count + 1u);
    for (unsigned long long each = 0ull; each < query_count; each += 1ull)
    {
        molecule_query_first[molecule_of_key[set.text[RANK_KEY][query[each]]] + 1ull] += 1ull;
    }
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        molecule_query_first[molecule + 1ull] += molecule_query_first[molecule];
    }
    {
        std::vector<unsigned long long> filled((size_t)molecule_count + 1u, 0ull);
        for (unsigned long long each = 0ull; each < query_count; each += 1ull)
        {
            const unsigned int molecule = molecule_of_key[set.text[RANK_KEY][query[each]]];
            molecule_query[molecule_query_first[molecule] + filled[molecule]] = (unsigned int)each;
            filled[molecule] += 1ull;
        }
    }
    rank_line_decimal(results, "  queries: ", query_count);
    rank_line_decimal(results, " spectra of ", molecule_count);
    scriptura_text(&results->line, " molecules");
    rank_line_end(results);

    // the query adducts, each read once
    std::vector<CasmiAdduct> adduct;
    std::vector<unsigned int> adduct_name;
    std::vector<unsigned int> query_adduct((size_t)query_count + 1u, RANK_ABSENT);
    unsigned long long refused_adduct = 0ull;
    for (unsigned long long each = 0ull; each < query_count; each += 1ull)
    {
        const unsigned int name = set.text[RANK_ADDUCT][query[each]];
        size_t slot = 0u;
        while ((slot < adduct_name.size()) && (adduct_name[slot] != name))
        {
            slot += 1u;
        }
        if ((slot == adduct_name.size()) && (name != RANK_ABSENT) && (slot < RANK_ADDUCTS_MOST))
        {
            CasmiAdduct read;
            memset(&read, 0, sizeof(read));
            const std::string &text = set.strings[RANK_ADDUCT][name];
            const CasmiAdductRead request = {text.data(), text.size(), &read};
            if (casmi_adduct_read(&request) == ENGINE_BYTES_ERROR)
            {
                read.charge = 0ll;
            }
            adduct.push_back(read);
            adduct_name.push_back(name);
        }
        const int readable = (slot < adduct.size()) && (adduct[slot].charge != 0ll);
        query_adduct[each] = readable ? (unsigned int)slot : RANK_ABSENT;
        refused_adduct += readable ? 0ull : 1ull;
    }
    rank_line_decimal(results, "  query adducts: ", adduct.size());
    rank_line_decimal(results, " read; query spectra whose adduct was refused ", refused_adduct);
    rank_line_end(results);

    // the candidate formulas: every structure holding a reference spectrum, its formula the first its spectra name
    std::vector<unsigned int> formula_of_structure((size_t)structures + 1u, RANK_ABSENT);
    std::vector<unsigned int> formula_text_of_structure((size_t)structures + 1u, RANK_ABSENT);
    for (unsigned long long spectrum = 0ull; spectrum < spectra; spectrum += 1ull)
    {
        const unsigned int structure = set.text[RANK_KEY][spectrum];
        if ((structure != RANK_ABSENT) && (formula_text_of_structure[structure] == RANK_ABSENT))
        {
            formula_text_of_structure[structure] = set.text[RANK_FORMULA][spectrum];
        }
    }
    const unsigned long long formula_texts = set.strings[RANK_FORMULA].size();
    std::vector<unsigned int> window_of_text((size_t)formula_texts + 1u, RANK_ABSENT);
    std::vector<CasmiFormula> formula;
    std::vector<unsigned int> formula_text;
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
            memset(&read, 0, sizeof(read));
            const std::string &held = set.strings[RANK_FORMULA][text];
            const CasmiFormulaRead request = {held.data(), held.size(), &read};
            int good = !held.empty() && (casmi_formula_read(&request) >= 0ll) && (read.element_count > 0u);
            for (unsigned int element = 0u; good && (element < read.element_count); element += 1u)
            {
                good = read.count[element] >= 0ll;
            }
            if (good)
            {
                window_of_text[text] = (unsigned int)formula.size();
                formula.push_back(read);
                formula_text.push_back(text);
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
    const unsigned long long formula_count = formula.size();
    std::vector<unsigned long long> formula_structure_first((size_t)formula_count + 2u, 0ull);
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
    std::vector<unsigned int> formula_structure((size_t)formula_structure_first[formula_count] + 1u);
    {
        std::vector<unsigned long long> filled((size_t)formula_count + 1u, 0ull);
        for (unsigned long long structure = 0ull; structure < structures; structure += 1ull)
        {
            const unsigned int each = formula_of_structure[structure];
            if (each != RANK_ABSENT)
            {
                formula_structure[formula_structure_first[each] + filled[each]] = (unsigned int)structure;
                filled[each] += 1ull;
            }
        }
    }
    rank_line_decimal(results, "  candidate structures: ", candidate_structures);
    rank_line_decimal(results, "; formulas read ", formula_count);
    rank_line_decimal(results, ", refused ", refused_formula);
    rank_line_end(results);

    // ---- ion ----
    unsigned int isotope_count = 0u;
    const CasmiIsotope *const isotope = casmi_isotope_table(&isotope_count);
    unsigned int terms_most = 0u;
    unsigned int molecules_bits = 1u;
    unsigned int charge_bits = 1u;
    unsigned int formula_bits = 1u;
    unsigned int adduct_bits = 1u;
    unsigned int mass_bits = 1u;
    for (size_t slot = 0u; slot < adduct.size(); slot += 1u)
    {
        if (adduct[slot].charge == 0ll)
        {
            continue;
        }
        molecules_bits = std::max(molecules_bits, rank_magnitude_bits(adduct[slot].molecules));
        charge_bits = std::max(charge_bits, rank_magnitude_bits(adduct[slot].charge));
        for (unsigned long long each = 0ull; each < formula_count; each += 1ull)
        {
            RankTerm term[RANK_TERMS_MOST];
            const unsigned int terms = rank_terms_merge(&formula[each], &adduct[slot], term);
            terms_most = std::max(terms_most, terms);
            for (unsigned int entry = 0u; entry < terms; entry += 1u)
            {
                mass_bits = std::max(mass_bits, rank_magnitude_bits(isotope[term[entry].isotope].mass));
                formula_bits = std::max(formula_bits, rank_magnitude_bits(term[entry].formula));
                adduct_bits = std::max(adduct_bits, rank_magnitude_bits(term[entry].adduct));
            }
        }
    }
    // the ion record: molecules, charge, then each term's formula count, adduct count and mass, each signed
    const unsigned int ion_term_bits = (formula_bits + 1u) + (adduct_bits + 1u) + (mass_bits + 1u);
    const unsigned int ion_atom_bits = (molecules_bits + 1u) + (charge_bits + 1u) + (terms_most * ion_term_bits);
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
        for (unsigned int term = 0u; term < terms_most; term += 1u)
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
    // the precursor record: its numerator and its code
    const unsigned int precursor_limbs = 3u;
    const unsigned int precursor_numerator_bits = 62u;
    RankMachine window;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int numerator_field = exact_record_member_field(&program, precursor_numerator_bits, 0u);
        const unsigned int code_field = exact_record_member_field(&program, RANK_CODE_BITS, 64u);
        const unsigned int mass_field = exact_record_member_field(&program, ion_mass.bits, ion_mass.offset);
        const unsigned int charge_field = exact_record_member_field(&program, ion_charge.bits, ion_charge.offset);
        unsigned int fields[RANK_ENVELOPE_FIELDS];
        rank_envelope_fields(&program, &envelope_layout, fields);
        const unsigned int numerator = exact_record_read_unsigned(&program, numerator_field, 0u);
        const unsigned int denominator =
            rank_denominator_steps(&program, exact_record_read_unsigned(&program, code_field, 0u));
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
    std::vector<unsigned long long> inside_first((size_t)query_count + 1u, 0ull);
    std::vector<unsigned long long> inside_count((size_t)query_count + 1u, 0ull);
    std::vector<unsigned int> inside;
    std::vector<unsigned int> ion_atoms((size_t)((formula_count + 1ull) * ion_atom_limbs), 0u);
    std::vector<unsigned int> ion_records((size_t)((formula_count + 1ull) * ion.layout.out_limbs), 0u);
    RankTally ion_tally = {0ull, 0ull};
    RankTally window_tally = {0ull, 0ull};
    unsigned long long ion_against_c = 0ull;
    unsigned long long window_mixed = 0ull;
    for (size_t slot = 0u; ok && (slot < adduct.size()); slot += 1u)
    {
        if (adduct[slot].charge == 0ll)
        {
            continue;
        }
        std::fill(ion_atoms.begin(), ion_atoms.end(), 0u);
        for (unsigned long long each = 0ull; each < formula_count; each += 1ull)
        {
            unsigned int *const atom = &ion_atoms[each * ion_atom_limbs];
            RankTerm term[RANK_TERMS_MOST];
            const unsigned int terms = rank_terms_merge(&formula[each], &adduct[slot], term);
            AnchorExactInteger value;
            unsigned int at = 0u;
            rank_exact_signed(adduct[slot].molecules, &value);
            rank_field_put(atom, at, molecules_bits + 1u, &value);
            at += molecules_bits + 1u;
            rank_exact_signed(adduct[slot].charge, &value);
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
        const unsigned int *const ion_in[ENGINE_RECORD_MEMBERS_MAX] = {ion_atoms.data(), NULL, NULL};
        const unsigned long long ion_bodies[ENGINE_RECORD_MEMBERS_MAX] = {formula_count, 0ull, 0ull};
        ok = rank_sweep(results, "ion", &ion, ion_in, ion_bodies, NULL, formula_count, ion_records.data(),
                        &ion_tally.microseconds);
        ion_tally.lanes += formula_count;
        // the engine's own C sum beside the record machine's, for every formula
        for (unsigned long long each = 0ull; ok && (each < formula_count); each += 1ull)
        {
            const long long neutral = casmi_formula_mass(&formula[each]);
            const long long numerator = (neutral < 0ll) ? -1ll : casmi_ion_numerator(neutral, &adduct[slot]);
            const long long expected = (numerator < 0ll) ? -1ll : (numerator + (adduct[slot].charge * CASMI_ELECTRON_MASS));
            AnchorExactInteger held;
            rank_field_read(&ion_records[each * ion.layout.out_limbs], ion_sum, &held);
            AnchorExactInteger wanted;
            rank_exact_signed(expected, &wanted);
            ion_against_c += ((expected >= 0ll) && !anchor_exact_equal(&held, &wanted)) ? 1ull : 0ull;
        }
        // the queries of this adduct against every formula, as many at a time as fill a chunk
        unsigned long long each = 0ull;
        const unsigned long long queries_most = (RANK_CHUNK_LANES / (formula_count + 1ull)) + 1ull;
        while (ok && (each < query_count))
        {
            std::vector<unsigned int> query_atoms;
            std::vector<unsigned int> window_query;
            while ((each < query_count) && (window_query.size() < queries_most))
            {
                if (query_adduct[each] == slot)
                {
                    const unsigned long long spectrum = query[each];
                    unsigned long long numerator = 0ull;
                    unsigned int code = 0u;
                    rank_rational(&set.precursor, spectrum, set.precursor.row_start[spectrum], RANK_UNIT_PLACES_MZ,
                                  &numerator, &code);
                    const size_t base_at = query_atoms.size();
                    query_atoms.resize(base_at + precursor_limbs, 0u);
                    rank_put_unsigned(&query_atoms[base_at], 0u, precursor_numerator_bits, numerator);
                    rank_put_unsigned(&query_atoms[base_at], 64u, RANK_CODE_BITS, code);
                    window_query.push_back((unsigned int)each);
                }
                each += 1ull;
            }
            const unsigned long long taken = window_query.size();
            if (taken == 0ull)
            {
                continue;
            }
            std::vector<unsigned int> index;
            index.reserve((size_t)(3ull * taken * formula_count));
            for (unsigned long long local = 0ull; local < taken; local += 1ull)
            {
                const unsigned int held_mode = mode[query[window_query[local]]];
                for (unsigned long long candidate = 0ull; candidate < formula_count; candidate += 1ull)
                {
                    index.push_back((unsigned int)local);
                    index.push_back((unsigned int)candidate);
                    index.push_back(held_mode);
                }
            }
            const unsigned long long lanes = taken * formula_count;
            std::vector<unsigned int> signs((size_t)((lanes + 1ull) * window.layout.out_limbs), 0u);
            const unsigned int *const window_in[ENGINE_RECORD_MEMBERS_MAX] = {query_atoms.data(), ion_records.data(),
                                                                             envelope_records.data()};
            const unsigned long long window_bodies[ENGINE_RECORD_MEMBERS_MAX] = {taken, formula_count, RANK_MODES};
            ok = rank_sweep(results, "window", &window, window_in, window_bodies, index.data(), lanes, signs.data(),
                            &window_tally.microseconds);
            window_tally.lanes += lanes;
            unsigned long long lane = 0ull;
            for (unsigned long long local = 0ull; ok && (local < taken); local += 1ull)
            {
                const unsigned int held = window_query[local];
                inside_first[held] = inside.size();
                for (unsigned long long candidate = 0ull; candidate < formula_count; candidate += 1ull)
                {
                    const unsigned int *const record = &signs[lane * window.layout.out_limbs];
                    const int high = rank_field_sign(record, window_high);
                    const int low = rank_field_sign(record, window_low);
                    window_mixed += ((high == 0) || (low == 0)) ? 1ull : 0ull;
                    if ((high < 0) && (low > 0))
                    {
                        inside.push_back((unsigned int)candidate);
                        inside_count[held] += 1ull;
                    }
                    lane += 1ull;
                }
            }
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
    rank_line_decimal(results, "  window: (query, formula) inside ", inside.size());
    rank_line_decimal(results, "; queries holding none ", queries_empty);
    rank_line_decimal(results, "; signs of 0 ", window_mixed);
    rank_line_end(results);

    // ---- peaks ----
    std::vector<unsigned int> intensity_flat;
    unsigned int intensity_limbs = 1u;
    // Each spectrum's intensities on its row's one scale. A row of one form takes its integers as they are, each below
    // 2^60; a row of no one form has its integers on the row's least common denominator, written into a side array at
    // the widest of them, its rows read once for the width and once to write them
    unsigned int intensity_bits = 61u;
    std::vector<unsigned long long> side_first((size_t)spectra + 1u, ~0ull);
    unsigned long long side_values = 0ull;
    for (unsigned int pass = 0u; ok && (pass < 2u); pass += 1u)
    {
        if (pass == 1u)
        {
            intensity_limbs = (intensity_bits + 31u) / 32u;
            intensity_flat.assign((size_t)(side_values * intensity_limbs), 0u);
            side_values = 0ull;
        }
        for (unsigned long long spectrum = 0ull; ok && (spectrum < spectra); spectrum += 1ull)
        {
            const int each_form = (set.intensity.term[spectrum] % RANK_TERM_FORMS) == RANK_ROW_EACH;
            if ((!reference[spectrum] && (set.text[RANK_LIBRARY][spectrum] != held_out)) || !each_form)
            {
                continue;
            }
            std::vector<AnchorExactInteger> row;
            ok = rank_intensity_row(&set, spectrum, &row);
            side_first[spectrum] = side_values;
            for (size_t peak = 0u; ok && (peak < row.size()); peak += 1u)
            {
                intensity_bits = (pass == 0u) ? std::max(intensity_bits, rank_bits(&row[peak]) + 1u) : intensity_bits;
                if (pass == 1u)
                {
                    memcpy(&intensity_flat[(side_values + peak) * intensity_limbs], row[peak].limb,
                           intensity_limbs * sizeof(unsigned int));
                }
            }
            side_values += row.size();
        }
    }
    if (!ok)
    {
        scriptura_text(&results->line, "  an intensity row did not come to one scale");
        rank_line_end(results);
        return 0;
    }
    RankPeakLayout peak_layout;
    peak_layout.numerator_bits = 62u;
    peak_layout.code_offset = 64u;
    peak_layout.intensity_offset = 96u;
    peak_layout.intensity_bits = intensity_bits;
    peak_layout.limbs = (peak_layout.intensity_offset + intensity_bits + 31u) / 32u;
    rank_line_decimal(results, "  peaks: intensity field ", intensity_bits);
    rank_line_decimal(results, " bits, record ", peak_layout.limbs);
    scriptura_text(&results->line, " limbs");
    rank_line_end(results);
    // a spectrum's peaks as records: each m/z's rational and its intensity
    auto peak_put = [&](unsigned long long spectrum, unsigned int *atoms) {
        const unsigned long long first = set.mz.row_start[spectrum];
        const unsigned long long past = set.mz.row_start[spectrum + 1ull];
        for (unsigned long long value = first; value < past; value += 1ull)
        {
            unsigned int *const atom = &atoms[(value - first) * peak_layout.limbs];
            unsigned long long numerator = 0ull;
            unsigned int code = 0u;
            rank_rational(&set.mz, spectrum, value, RANK_UNIT_PLACES_MZ, &numerator, &code);
            rank_put_unsigned(atom, 0u, peak_layout.numerator_bits, numerator);
            rank_put_unsigned(atom, peak_layout.code_offset, RANK_CODE_BITS, code);
            if (side_first[spectrum] != ~0ull)
            {
                rank_bits_copy(atom, peak_layout.intensity_offset,
                               &intensity_flat[(side_first[spectrum] + (value - first)) * intensity_limbs],
                               std::min(peak_layout.intensity_bits, 32u * intensity_limbs));
            }
            else
            {
                rank_put_unsigned(atom, peak_layout.intensity_offset, peak_layout.intensity_bits, set.intensity.unit[value]);
            }
        }
    };
    RankMachine match;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int numerator_field = exact_record_member_field(&program, peak_layout.numerator_bits, 0u);
        const unsigned int code_field = exact_record_member_field(&program, RANK_CODE_BITS, peak_layout.code_offset);
        const unsigned int intensity_field =
            exact_record_member_field(&program, peak_layout.intensity_bits, peak_layout.intensity_offset);
        unsigned int fields[RANK_ENVELOPE_FIELDS];
        rank_envelope_fields(&program, &envelope_layout, fields);
        const unsigned int query_n = exact_record_read_unsigned(&program, numerator_field, 0u);
        const unsigned int reference_n = exact_record_read_unsigned(&program, numerator_field, 1u);
        const unsigned int query_d = rank_denominator_steps(&program, exact_record_read_unsigned(&program, code_field, 0u));
        const unsigned int reference_d =
            rank_denominator_steps(&program, exact_record_read_unsigned(&program, code_field, 1u));
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
        unsigned int weight = exact_record_product(
            &program, inside,
            exact_record_product(&program, exact_record_read(&program, intensity_field, 0u),
                                 exact_record_read(&program, intensity_field, 1u)));
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {peak_layout.limbs, peak_layout.limbs,
                                                               envelope_layout.limbs};
        ok = rank_machine_load(results, "match", &program, &weight, 1u, 3u, limbs, &match);
        exact_record_close(&program);
        if (!ok)
        {
            return 0;
        }
    }
    const RankField weight_field = rank_output(&match, 0u);
    const unsigned int match_limbs = match.layout.out_limbs;

    // match: each query's peaks against every peak of every reference spectrum its window names, same mode
    std::vector<unsigned int> matched;
    std::vector<unsigned int> matched_group_first;
    std::vector<unsigned int> pair_query;
    std::vector<unsigned int> pair_spectrum;
    std::vector<unsigned int> pair_group_first;
    std::vector<unsigned long long> query_pair_first((size_t)query_count + 1u, 0ull);
    std::vector<unsigned long long> query_pair_count((size_t)query_count + 1u, 0ull);
    RankTally match_tally = {0ull, 0ull};
    for (unsigned long long each = 0ull; ok && (each < query_count); each += 1ull)
    {
        const unsigned int spectrum_q = query[each];
        const unsigned int held_mode = mode[spectrum_q];
        const unsigned long long query_peaks = set.mz.row_start[spectrum_q + 1ull] - set.mz.row_start[spectrum_q];
        query_pair_first[each] = pair_query.size();
        if ((query_peaks == 0ull) || (inside_count[each] == 0ull))
        {
            continue;
        }
        std::vector<unsigned int> query_atoms((size_t)(query_peaks * peak_layout.limbs), 0u);
        peak_put(spectrum_q, query_atoms.data());
        std::vector<unsigned int> query_reference;
        for (unsigned long long slot = 0ull; slot < inside_count[each]; slot += 1ull)
        {
            const unsigned int named = inside[inside_first[each] + slot];
            for (unsigned long long at = formula_structure_first[named]; at < formula_structure_first[named + 1ull];
                 at += 1ull)
            {
                const unsigned int structure = formula_structure[at];
                for (unsigned long long spectrum_at = structure_reference_first[structure];
                     spectrum_at < structure_reference_first[structure + 1ull]; spectrum_at += 1ull)
                {
                    const unsigned int spectrum = structure_reference[spectrum_at];
                    const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                    if ((mode[spectrum] == held_mode) && (peaks > 0ull))
                    {
                        query_reference.push_back(spectrum);
                    }
                }
            }
        }
        size_t start = 0u;
        while (ok && (start < query_reference.size()))
        {
            // a chunk: reference spectra until their lanes would pass the chunk, at least one
            std::vector<unsigned int> chunk;
            unsigned long long lanes = 0ull;
            unsigned long long reference_peaks = 0ull;
            while (start < query_reference.size())
            {
                const unsigned int spectrum = query_reference[start];
                const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                if (!chunk.empty() && ((lanes + (query_peaks * peaks)) > RANK_CHUNK_LANES))
                {
                    break;
                }
                chunk.push_back(spectrum);
                lanes += query_peaks * peaks;
                reference_peaks += peaks;
                start += 1u;
            }
            std::vector<unsigned int> reference_atoms((size_t)(reference_peaks * peak_layout.limbs), 0u);
            std::vector<unsigned int> index;
            index.reserve((size_t)(3ull * lanes));
            unsigned long long atom = 0ull;
            for (size_t local = 0u; local < chunk.size(); local += 1u)
            {
                const unsigned int spectrum = chunk[local];
                const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                peak_put(spectrum, &reference_atoms[atom * peak_layout.limbs]);
                for (unsigned long long query_peak = 0ull; query_peak < query_peaks; query_peak += 1ull)
                {
                    for (unsigned long long peak = 0ull; peak < peaks; peak += 1ull)
                    {
                        index.push_back((unsigned int)query_peak);
                        index.push_back((unsigned int)(atom + peak));
                        index.push_back(held_mode);
                    }
                }
                atom += peaks;
            }
            std::vector<unsigned int> out((size_t)((lanes + 1ull) * match_limbs), 0u);
            const unsigned int *const match_in[ENGINE_RECORD_MEMBERS_MAX] = {query_atoms.data(), reference_atoms.data(),
                                                                            envelope_records.data()};
            const unsigned long long match_bodies[ENGINE_RECORD_MEMBERS_MAX] = {query_peaks, reference_peaks, RANK_MODES};
            ok = rank_sweep(results, "match", &match, match_in, match_bodies, index.data(), lanes, out.data(),
                            &match_tally.microseconds);
            match_tally.lanes += lanes;
            // every lane whose weight is above zero is kept; a run of one (reference, query peak) is one group
            unsigned long long lane = 0ull;
            for (size_t local = 0u; ok && (local < chunk.size()); local += 1u)
            {
                const unsigned int spectrum = chunk[local];
                const unsigned long long peaks = set.mz.row_start[spectrum + 1ull] - set.mz.row_start[spectrum];
                int pair_open = 0;
                for (unsigned long long query_peak = 0ull; query_peak < query_peaks; query_peak += 1ull)
                {
                    int group_open = 0;
                    for (unsigned long long peak = 0ull; peak < peaks; peak += 1ull)
                    {
                        const unsigned int *const record = &out[lane * match_limbs];
                        lane += 1ull;
                        if (rank_field_sign(record, weight_field) <= 0)
                        {
                            continue;
                        }
                        if (!pair_open)
                        {
                            pair_query.push_back((unsigned int)each);
                            pair_spectrum.push_back(spectrum);
                            pair_group_first.push_back((unsigned int)matched_group_first.size());
                            query_pair_count[each] += 1ull;
                            pair_open = 1;
                        }
                        if (!group_open)
                        {
                            matched_group_first.push_back((unsigned int)(matched.size() / match_limbs));
                            group_open = 1;
                        }
                        matched.insert(matched.end(), record, record + match_limbs);
                    }
                }
            }
        }
    }
    rank_machine_release(&match);
    if (!ok)
    {
        return 0;
    }
    const unsigned long long matched_count = matched.size() / match_limbs;
    const unsigned long long pair_count = pair_query.size();
    const unsigned long long peak_group_count = matched_group_first.size();
    rank_tally_line(results, "match", &match_tally);
    rank_line_decimal(results, "  match: pairs holding a matched peak ", pair_count);
    rank_line_decimal(results, "; (pair, query peak) groups ", peak_group_count);
    rank_line_decimal(results, "; matched lanes kept ", matched_count);
    rank_line_end(results);

    // peak: each (pair, query peak) group's heaviest weight
    RankMachine value_order;
    ok = rank_value_order_lay(results, weight_field, match_limbs, &value_order);
    std::vector<unsigned long long> peak_first((size_t)peak_group_count + 1u);
    std::vector<unsigned long long> peak_count((size_t)peak_group_count + 1u);
    std::vector<unsigned int> matched_entry((size_t)matched_count + 1u);
    std::vector<unsigned int> peak_winner((size_t)peak_group_count + 1u);
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
    const RankGroups peak_groups = {peak_first.data(), peak_count.data(), matched_entry.data(), peak_group_count};
    ok = ok && rank_group_best(results, "peak", &value_order, rank_output(&value_order, 0u), matched.data(), matched_count,
                               &peak_groups, peak_winner.data(), &peak_tally.microseconds);
    rank_machine_release(&value_order);
    rank_tally_line(results, "peak tournament", &peak_tally);

    // sum: M per pair, the sum of its query peaks' kept weights
    std::vector<unsigned long long> pair_first((size_t)pair_count + 1u);
    std::vector<unsigned long long> pair_peaks((size_t)pair_count + 1u);
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        pair_first[pair] = pair_group_first[pair];
        const unsigned long long past = ((pair + 1ull) < pair_count) ? pair_group_first[pair + 1ull] : peak_group_count;
        pair_peaks[pair] = past - pair_first[pair];
    }
    RankTally weight_tally = {0ull, 0ull};
    const RankGroups pair_groups = {pair_first.data(), pair_peaks.data(), peak_winner.data(), pair_count};
    std::vector<unsigned int> weight_sums;
    RankField weight_sum_field;
    unsigned int weight_sum_limbs = 0u;
    ok = ok && rank_group_sum(results, "weight", weight_field, match_limbs, matched.data(), matched_count, &pair_groups,
                              &weight_sums, &weight_sum_field, &weight_sum_limbs, &weight_tally.microseconds);
    rank_tally_line(results, "weight sums", &weight_tally);

    // norm: Q and R, the sum of I^2 over each spectrum a pair names
    std::vector<unsigned int> norm_of_reference((size_t)spectra + 1u, RANK_ABSENT);
    std::vector<unsigned int> norm_of_query((size_t)query_count + 1u, RANK_ABSENT);
    std::vector<unsigned int> norm_spectrum;
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        const unsigned int held_query = pair_query[pair];
        const unsigned int held_reference = pair_spectrum[pair];
        if (norm_of_query[held_query] == RANK_ABSENT)
        {
            norm_of_query[held_query] = (unsigned int)norm_spectrum.size();
            norm_spectrum.push_back(query[held_query]);
        }
        if (norm_of_reference[held_reference] == RANK_ABSENT)
        {
            norm_of_reference[held_reference] = (unsigned int)norm_spectrum.size();
            norm_spectrum.push_back(held_reference);
        }
    }
    unsigned long long norm_peaks = 0ull;
    for (size_t each = 0u; each < norm_spectrum.size(); each += 1u)
    {
        norm_peaks += set.mz.row_start[norm_spectrum[each] + 1ull] - set.mz.row_start[norm_spectrum[each]];
    }
    RankMachine norm;
    {
        ExactRecordProgram program;
        exact_record_open(&program);
        const unsigned int intensity_field =
            exact_record_member_field(&program, peak_layout.intensity_bits, peak_layout.intensity_offset);
        const unsigned int intensity = exact_record_read(&program, intensity_field, 0u);
        unsigned int square = exact_record_product(&program, intensity, intensity);
        const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {peak_layout.limbs, 0u, 0u};
        ok = ok && rank_machine_load(results, "norm", &program, &square, 1u, 1u, limbs, &norm);
        exact_record_close(&program);
    }
    std::vector<unsigned int> norm_atoms((size_t)((norm_peaks + 1ull) * peak_layout.limbs), 0u);
    std::vector<unsigned long long> norm_first(norm_spectrum.size() + 1u);
    std::vector<unsigned long long> norm_count(norm_spectrum.size() + 1u);
    std::vector<unsigned int> norm_entry((size_t)norm_peaks + 1u);
    unsigned long long norm_atom = 0ull;
    for (size_t each = 0u; each < norm_spectrum.size(); each += 1u)
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
    std::vector<unsigned int> norm_squares((size_t)((norm_peaks + 1ull) * norm_limbs), 0u);
    RankTally square_tally = {norm_peaks, 0ull};
    const unsigned int *const norm_in[ENGINE_RECORD_MEMBERS_MAX] = {norm_atoms.data(), NULL, NULL};
    const unsigned long long norm_bodies[ENGINE_RECORD_MEMBERS_MAX] = {norm_peaks, 0ull, 0ull};
    ok = ok && rank_sweep(results, "norm", &norm, norm_in, norm_bodies, NULL, norm_peaks, norm_squares.data(),
                          &square_tally.microseconds);
    const RankField square_field = ok ? rank_output(&norm, 0u) : RankField();
    rank_machine_release(&norm);
    RankTally norm_tally = {0ull, 0ull};
    const RankGroups norm_groups = {norm_first.data(), norm_count.data(), norm_entry.data(), norm_spectrum.size()};
    std::vector<unsigned int> norm_sums;
    RankField norm_sum_field;
    unsigned int norm_sum_limbs = 0u;
    ok = ok && rank_group_sum(results, "norm", square_field, norm_limbs, norm_squares.data(), norm_peaks, &norm_groups,
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
    std::vector<unsigned int> score_index((size_t)(3ull * pair_count) + 3u);
    std::vector<unsigned int> scores((size_t)((pair_count + 1ull) * score_limbs), 0u);
    for (unsigned long long pair = 0ull; pair < pair_count; pair += 1ull)
    {
        score_index[3ull * pair] = (unsigned int)pair;
        score_index[(3ull * pair) + 1ull] = norm_of_query[pair_query[pair]];
        score_index[(3ull * pair) + 2ull] = norm_of_reference[pair_spectrum[pair]];
    }
    RankTally score_tally = {pair_count, 0ull};
    const unsigned int *const score_in[ENGINE_RECORD_MEMBERS_MAX] = {weight_sums.data(), norm_sums.data(),
                                                                    norm_sums.data()};
    const unsigned long long score_bodies[ENGINE_RECORD_MEMBERS_MAX] = {pair_count + 1ull, norm_spectrum.size() + 1ull,
                                                                       norm_spectrum.size() + 1ull};
    ok = ok && rank_sweep(results, "score", &score, score_in, score_bodies, score_index.data(), pair_count, scores.data(),
                          &score_tally.microseconds);
    const RankField score_numerator = ok ? rank_output(&score, 0u) : RankField();
    const RankField score_denominator = ok ? rank_output(&score, 1u) : RankField();
    rank_machine_release(&score);
    rank_tally_line(results, "score", &score_tally);
    RankMachine ratio_order;
    ok = ok && rank_ratio_order_lay(results, score_numerator, score_denominator, score_limbs, &ratio_order);
    if (!ok)
    {
        return 0;
    }
    const RankField order_sign = rank_output(&ratio_order, 0u);

    // structure: each (molecule, structure) group's heaviest pair
    std::vector<unsigned int> structure_group((size_t)structures + 1u, RANK_ABSENT);
    std::vector<unsigned int> group_structure;
    std::vector<unsigned int> group_entry;
    std::vector<unsigned long long> molecule_group_first((size_t)molecule_count + 2u, 0ull);
    std::vector<unsigned int> group_first_list;
    std::vector<unsigned int> group_count_list;
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        molecule_group_first[molecule] = group_structure.size();
        const size_t base = group_structure.size();
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int held = molecule_query[at];
            for (unsigned long long pair = query_pair_first[held]; pair < (query_pair_first[held] + query_pair_count[held]);
                 pair += 1ull)
            {
                const unsigned int structure = set.text[RANK_KEY][pair_spectrum[pair]];
                if (structure_group[structure] == RANK_ABSENT)
                {
                    structure_group[structure] = (unsigned int)(group_structure.size() - base);
                    group_structure.push_back(structure);
                    group_count_list.push_back(0u);
                }
                group_count_list[base + structure_group[structure]] += 1u;
            }
        }
        unsigned int running = (unsigned int)group_entry.size();
        for (size_t group = base; group < group_structure.size(); group += 1u)
        {
            group_first_list.push_back(running);
            running += group_count_list[group];
        }
        group_entry.resize(running, 0u);
        for (size_t group = base; group < group_structure.size(); group += 1u)
        {
            group_count_list[group] = 0u;
        }
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int held = molecule_query[at];
            for (unsigned long long pair = query_pair_first[held]; pair < (query_pair_first[held] + query_pair_count[held]);
                 pair += 1ull)
            {
                const unsigned int structure = set.text[RANK_KEY][pair_spectrum[pair]];
                const size_t group = base + structure_group[structure];
                group_entry[group_first_list[group] + group_count_list[group]] = (unsigned int)pair;
                group_count_list[group] += 1u;
            }
        }
        for (size_t group = base; group < group_structure.size(); group += 1u)
        {
            structure_group[group_structure[group]] = RANK_ABSENT;
        }
    }
    molecule_group_first[molecule_count] = group_structure.size();
    const unsigned long long group_count = group_structure.size();
    std::vector<unsigned long long> structure_first((size_t)group_count + 1u);
    std::vector<unsigned long long> structure_count((size_t)group_count + 1u);
    std::vector<unsigned int> structure_winner((size_t)group_count + 1u);
    for (unsigned long long group = 0ull; group < group_count; group += 1ull)
    {
        structure_first[group] = group_first_list[group];
        structure_count[group] = group_count_list[group];
    }
    RankTally structure_tally = {0ull, 0ull};
    const RankGroups structure_groups = {structure_first.data(), structure_count.data(), group_entry.data(), group_count};
    ok = rank_group_best(results, "structure", &ratio_order, order_sign, scores.data(), pair_count, &structure_groups,
                         structure_winner.data(), &structure_tally.microseconds);
    rank_tally_line(results, "structure tournament", &structure_tally);

    // list: per molecule, repeated tournaments over its structures' winning pairs, heaviest first
    std::vector<unsigned long long> list_first((size_t)molecule_count + 1u);
    std::vector<unsigned long long> list_count((size_t)molecule_count + 1u);
    std::vector<unsigned int> list_entry((size_t)group_count + 1u);
    std::vector<unsigned int> list_group((size_t)group_count + 1u);
    std::vector<unsigned int> list_winner((size_t)molecule_count + 1u);
    std::vector<unsigned int> chosen((size_t)(molecule_count * RANK_LIST_MOST) + 1u);
    std::vector<unsigned int> chosen_count((size_t)molecule_count + 1u, 0u);
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
    for (unsigned int pick = 0u; ok && (pick < RANK_LIST_MOST); pick += 1u)
    {
        const RankGroups list_groups = {list_first.data(), list_count.data(), list_entry.data(), molecule_count};
        ok = rank_group_best(results, "list", &ratio_order, order_sign, scores.data(), pair_count, &list_groups,
                             list_winner.data(), &list_tally.microseconds);
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
            chosen[(molecule * RANK_LIST_MOST) + chosen_count[molecule]] = list_group[base + at];
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

    // each molecule's list: the chosen structures, then its unscored candidates in structure order, to 25; the
    // answer's place in it
    std::vector<unsigned char> structure_mark((size_t)structures + 1u, 0u);
    unsigned long long rank_counts[RANK_LIST_MOST + 1u];
    memset(rank_counts, 0, sizeof(rank_counts));
    unsigned long long truth_no_reference = 0ull;
    unsigned long long truth_outside = 0ull;
    unsigned long long truth_unscored = 0ull;
    for (unsigned long long molecule = 0ull; molecule < molecule_count; molecule += 1ull)
    {
        std::vector<unsigned int> candidate;
        for (unsigned long long at = molecule_query_first[molecule]; at < molecule_query_first[molecule + 1ull]; at += 1ull)
        {
            const unsigned int held = molecule_query[at];
            for (unsigned long long slot = 0ull; slot < inside_count[held]; slot += 1ull)
            {
                const unsigned int named = inside[inside_first[held] + slot];
                for (unsigned long long each = formula_structure_first[named]; each < formula_structure_first[named + 1ull];
                     each += 1ull)
                {
                    const unsigned int structure = formula_structure[each];
                    if (structure_mark[structure] == 0u)
                    {
                        structure_mark[structure] = 1u;
                        candidate.push_back(structure);
                    }
                }
            }
        }
        std::sort(candidate.begin(), candidate.end());
        unsigned int list[RANK_LIST_MOST];
        unsigned int listed = 0u;
        for (unsigned int place = 0u; place < chosen_count[molecule]; place += 1u)
        {
            const unsigned int structure = group_structure[chosen[(molecule * RANK_LIST_MOST) + place]];
            list[listed] = structure;
            listed += 1u;
            structure_mark[structure] = 2u;
        }
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            structure_mark[group_structure[group]] = 2u;
        }
        for (size_t each = 0u; (each < candidate.size()) && (listed < RANK_LIST_MOST); each += 1u)
        {
            if (structure_mark[candidate[each]] == 1u)
            {
                list[listed] = candidate[each];
                listed += 1u;
            }
        }
        const unsigned int truth = molecule_key[molecule];
        unsigned int place = 0u;
        for (unsigned int at = 0u; at < listed; at += 1u)
        {
            place = ((place == 0u) && (list[at] == truth)) ? (at + 1u) : place;
        }
        rank_counts[place] += 1ull;
        const int has_reference = structure_reference_first[truth + 1ull] != structure_reference_first[truth];
        const int is_candidate = std::binary_search(candidate.begin(), candidate.end(), truth);
        int is_scored = 0;
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            is_scored = is_scored || (group_structure[group] == truth);
        }
        truth_no_reference += has_reference ? 0ull : 1ull;
        truth_outside += (has_reference && !is_candidate) ? 1ull : 0ull;
        truth_unscored += (is_candidate && !is_scored) ? 1ull : 0ull;
        for (size_t each = 0u; each < candidate.size(); each += 1u)
        {
            structure_mark[candidate[each]] = 0u;
        }
        for (unsigned long long group = molecule_group_first[molecule]; group < molecule_group_first[molecule + 1ull];
             group += 1ull)
        {
            structure_mark[group_structure[group]] = 0u;
        }
    }
    rank_line_decimal(results, "  answers: molecules ", molecule_count);
    rank_line_decimal(results, "; with no reference spectrum ", truth_no_reference);
    rank_line_decimal(results, "; formula outside every window ", truth_outside);
    rank_line_decimal(results, "; candidate with no matched peak ", truth_unscored);
    rank_line_end(results);
    scriptura_text(&results->line, "  answers at each place, 1 to 25, then not listed:");
    for (unsigned int place = 1u; place <= RANK_LIST_MOST; place += 1u)
    {
        rank_line_decimal(results, " ", rank_counts[place]);
    }
    rank_line_decimal(results, "; ", rank_counts[0]);
    rank_line_end(results);
    // MRR@25 = sum count_k / k over the molecules: each count_k times lcm(1..25) / k summed, over lcm(1..25) times the
    // molecules, read in parts per million
    unsigned long long numerator = 0ull;
    unsigned long long common = 1ull;
    for (unsigned int place = 1u; place <= RANK_LIST_MOST; place += 1u)
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
    for (unsigned int place = 1u; place <= RANK_LIST_MOST; place += 1u)
    {
        numerator += rank_counts[place] * (common / place);
    }
    const unsigned long long parts = (molecule_count != 0ull) ? ((numerator * 1000000ull) / (common * molecule_count)) : 0ull;
    rank_line_decimal(results, "  MRR@25: ", parts);
    scriptura_text(&results->line, " parts per million, floored");
    rank_line_end(results);
    return 1;
}
