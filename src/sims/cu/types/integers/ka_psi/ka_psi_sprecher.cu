// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ka_psi_sprecher.cu: Sprecher's construction and the scales
#include "ka_psi_internal.h"
#include "../../../../../../includes/platform/platform.h"

void psi_digits(unsigned long long index, unsigned long long base, unsigned int levels, unsigned char *digit)
{
    for (unsigned int at = levels; at > 0u; at -= 1u)
    {
        // a digit is below the base, which is far below 256
        digit[at - 1u] = (unsigned char)(index % base);
        index /= base;
    }
}

void psi_sprecher(SimResults *results, const PsiCase *psi_case)
{
    VerbaLinea *const line = &results->line;
    OriorExactInteger denominator;
    OriorExactInteger power;
    sim_rational_status_check(sim_exact_power(psi_case->base, psi_case->weight[PSI_EXHAUSTIVE], &power));
    psi_times_integer(&power, 1ULL << PSI_EXHAUSTIVE, &denominator);
    const unsigned char before[PSI_EXHAUSTIVE] = {5u, 8u, 9u, 9u, 9u};
    const unsigned char after[PSI_EXHAUSTIVE] = {5u, 9u, 0u, 0u, 0u};
    OriorExactInteger numerator;
    psi_sprecher_numerator(psi_case, before, &numerator);
    const SimRational low = psi_rational_exact(&numerator, &denominator);
    psi_sprecher_numerator(psi_case, after, &numerator);
    const SimRational high = psi_rational_exact(&numerator, &denominator);
    sim_check(results, sim_rational_equal(low, sim_rational(2207LL, 4000LL)),
              "Sprecher's psi(0.58999) is 0.55175 exactly (2.5)");
    sim_check(results, sim_rational_equal(high, sim_rational(11LL, 20LL)),
              "Sprecher's psi(0.59) is 0.55 exactly (2.5)");
    sim_check(results, sim_rational_sign(sim_rational_difference(low, high)) > 0, "so Sprecher's psi is not monotone");
    unsigned long long descents = 0ULL;
    unsigned long long first = 0ULL;
    OriorExactInteger previous;
    unsigned char digit[PSI_EXHAUSTIVE];
    const unsigned long long points = 100000ULL;
    for (unsigned long long index = 0ULL; index < points; index += 1ULL)
    {
        psi_digits(index, psi_case->base, PSI_EXHAUSTIVE, digit);
        psi_sprecher_numerator(psi_case, digit, &numerator);
        if ((index != 0ULL) && (orior_exact_compare(&numerator, &previous) < 0))
        {
            first = (descents == 0ULL) ? index : first;
            descents += 1ULL;
        }
        previous = numerator;
    }
    sim_check(results, descents != 0ULL, "Sprecher's psi descends on D_5");
    mmgr_line_put(line, "\n  1. Sprecher's psi (2.4), n = 2, gamma = 10: psi(0.58999) = ");
    sim_rational_print(line, low);
    mmgr_line_put(line, " and psi(0.59) = ");
    sim_rational_print(line, high);
    mmgr_line_put(line, " (the paper's (2.5), exact)\n     it descends between ");
    mmgr_line_decimal(line, descents, 1u);
    mmgr_line_put(line, " of the 99,999 neighboring pairs of D_5, the first at 0.");
    psi_digits(first, psi_case->base, PSI_EXHAUSTIVE, digit);
    for (unsigned int at = 0u; at < PSI_EXHAUSTIVE; at += 1u)
    {
        mmgr_line_ch(line, (char)('0' + digit[at]));
    }
    mmgr_line_ch(line, '\n');
    sim_flush(results);
}

void psi_scale_open(const PsiCase *psi_case, PsiScale *scale)
{
    for (unsigned int level = 1u; level <= PSI_EXHAUSTIVE; level += 1u)
    {
        OriorExactInteger power;
        sim_rational_status_check(sim_exact_power(
            psi_case->base, psi_case->weight[level] - psi_case->weight[level - 1u], &scale->lift[level]));
        psi_times_integer(&scale->lift[level], 2ULL, &scale->up[level]);
        sim_exact_unsigned(&scale->unit[level], 1ULL << level);
        sim_exact_unsigned(&scale->half[level], 1ULL << (level - 1u));
        sim_rational_status_check(sim_exact_power(psi_case->base, psi_case->weight[level], &power));
        psi_times_integer(&power, 1ULL << level, &scale->denominator[level]);
    }
}

// The scale step in numerators over D_L = 2^L . gamma^beta(L): (value, plus) at level L - 1 are raised by
// 2 gamma^(beta(L) - beta(L - 1)); a regular digit adds i units of 2^L; the carried midpoint is
// (value + plus) gamma^(beta(L) - beta(L - 1)) + tilt . 2^(L - 1), tilt being gamma - 2 (2.9) or gamma - 1 (2.7)
static void psi_pair_numerator(const PsiCase *psi_case, const PsiScale *scale, const unsigned char *digit,
                               unsigned int levels, unsigned long long tilt, OriorExactInteger *value,
                               OriorExactInteger *plus)
{
    const unsigned long long base = psi_case->base;
    sim_exact_unsigned(value, 2ULL * digit[0]);
    sim_exact_unsigned(plus, 2ULL * (digit[0] + 1ULL));
    for (unsigned int level = 2u; level <= levels; level += 1u)
    {
        const unsigned long long place = digit[level - 1u];
        OriorExactInteger raised;
        OriorExactInteger shifted;
        OriorExactInteger both;
        OriorExactInteger lifted;
        OriorExactInteger midpoint;
        if (place < (base - 2ULL))
        {
            psi_multiply(value, &scale->up[level], &raised);
            psi_times_integer(&scale->unit[level], place + 1ULL, &shifted);
            psi_add(&raised, &shifted, plus);
            psi_times_integer(&scale->unit[level], place, &shifted);
            psi_add(&raised, &shifted, value);
            continue;
        }
        psi_add(value, plus, &both);
        psi_multiply(&both, &scale->lift[level], &lifted);
        psi_times_integer(&scale->half[level], tilt, &shifted);
        psi_add(&lifted, &shifted, &midpoint);
        if (place == (base - 2ULL))
        {
            psi_multiply(value, &scale->up[level], &raised);
            psi_times_integer(&scale->unit[level], place, &shifted);
            psi_add(&raised, &shifted, value);
            *plus = midpoint;
        }
        else
        {
            psi_multiply(plus, &scale->up[level], &raised);
            *plus = raised;
            *value = midpoint;
        }
    }
}

// level L from level L - 1, point by point, as the paper's recursion reads: a regular digit adds to the prefix's
// value, and the last digit takes the prefix and its successor's average plus the carried units
void psi_table_next(const PsiCase *psi_case, const PsiScale *scale, unsigned int level, unsigned long long tilt,
                    const OriorExactInteger *below, unsigned long long below_count, OriorExactInteger *table)
{
    const unsigned long long base = psi_case->base;
    for (unsigned long long prefix = 0ULL; prefix < below_count; prefix += 1ULL)
    {
        OriorExactInteger raised;
        OriorExactInteger shifted;
        psi_multiply(&below[prefix], &scale->up[level], &raised);
        for (unsigned long long place = 0ULL; place < (base - 1ULL); place += 1ULL)
        {
            psi_times_integer(&scale->unit[level], place, &shifted);
            psi_add(&raised, &shifted, &table[(prefix * base) + place]);
        }
        OriorExactInteger both;
        OriorExactInteger lifted;
        psi_add(&below[prefix], &below[prefix + 1ULL], &both);
        psi_multiply(&both, &scale->lift[level], &lifted);
        psi_times_integer(&scale->half[level], tilt, &shifted);
        psi_add(&lifted, &shifted, &table[(prefix * base) + base - 1ULL]);
    }
    psi_multiply(&below[below_count], &scale->up[level], &table[below_count * base]);
}

static const char *psi_measurement(const PsiCase *psi_case, unsigned long long tilt)
{
    return (tilt == (psi_case->base - 2ULL)) ? "(2.9)/(2.10), the form the proofs use" : "(2.7) as printed";
}

// the carried midpoint splits a gap D into D/2 - s u/2 on each side, s = tilt and 2(gamma - 2) - tilt
unsigned long long psi_wide_side(const PsiCase *psi_case, unsigned long long tilt)
{
    const unsigned long long other = (2ULL * (psi_case->base - 2ULL)) - tilt;
    return (tilt < other) ? tilt : other;
}

void psi_exhaustive(SimResults *results, const PsiCase *psi_case, const PsiScale *scale, unsigned long long tilt,
                    OriorExactInteger *measured_widest)
{
    VerbaLinea *const line = &results->line;
    const unsigned long long base = psi_case->base;
    unsigned long long max = 1ULL;
    for (unsigned int level = 0u; level < PSI_EXHAUSTIVE; level += 1u)
    {
        max *= base;
    }
    OriorExactInteger *below =
        (OriorExactInteger *)platform_cell_taken((unsigned long long)((size_t)(max + 1ULL) * sizeof(OriorExactInteger)),
                                                 (unsigned int)platform_place_g_program_owner, 0);
    OriorExactInteger *table =
        (OriorExactInteger *)platform_cell_taken((unsigned long long)((size_t)(max + 1ULL) * sizeof(OriorExactInteger)),
                                                 (unsigned int)platform_place_g_program_owner, 0);
    if ((below == NULL) || (table == NULL))
    {
        sim_check(results, 0, "the level tables were allocated");
        platform_cell_released(below, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(table, (unsigned int)platform_place_g_program_owner);
        return;
    }
    for (unsigned long long index = 0ULL; index <= base; index += 1ULL)
    {
        sim_exact_unsigned(&table[index], 2ULL * index);
    }
    OriorExactInteger predicted;
    sim_exact_unsigned(&predicted, 2ULL);
    const unsigned long long side = psi_wide_side(psi_case, tilt);
    unsigned long long points = base;
    mmgr_line_put(line, "     reading ");
    mmgr_line_put(line, psi_measurement(psi_case, tilt));
    mmgr_line_put(
        line,
        ":\n     level   points   step = recursion   increasing   least gap = g^-beta(L)   widest gap = predicted\n");
    for (unsigned int level = 1u; level <= PSI_EXHAUSTIVE; level += 1u)
    {
        if (level >= 2u)
        {
            OriorExactInteger *const swap = below;
            below = table;
            table = swap;
            psi_table_next(psi_case, scale, level, tilt, below, points, table);
            points *= base;
            OriorExactInteger lifted;
            OriorExactInteger shrink;
            OriorExactInteger next;
            psi_multiply(&predicted, &scale->lift[level], &lifted);
            psi_times_integer(&scale->half[level], side, &shrink);
            sim_rational_status_check(sim_exact_less(&lifted, &shrink, &next));
            predicted = next;
        }
        unsigned long long matched = 0ULL;
        unsigned long long increasing = 0ULL;
        OriorExactInteger least;
        OriorExactInteger widest;
        unsigned char digit[PSI_EXHAUSTIVE];
        for (unsigned long long index = 0ULL; index < points; index += 1ULL)
        {
            OriorExactInteger value;
            OriorExactInteger plus;
            OriorExactInteger gap;
            psi_digits(index, base, level, digit);
            psi_pair_numerator(psi_case, scale, digit, level, tilt, &value, &plus);
            matched += ((orior_exact_compare(&value, &table[index]) == 0) &&
                        (orior_exact_compare(&plus, &table[index + 1ULL]) == 0))
                           ? 1ULL
                           : 0ULL;
            sim_rational_status_check(sim_exact_less(&table[index + 1ULL], &table[index], &gap));
            increasing += (gap.sign > 0) ? 1ULL : 0ULL;
            if ((index == 0ULL) || (orior_exact_compare(&gap, &least) < 0))
            {
                least = gap;
            }
            if ((index == 0ULL) || (orior_exact_compare(&gap, &widest) > 0))
            {
                widest = gap;
            }
        }
        const int least_is_unit = (orior_exact_compare(&least, &scale->unit[level]) == 0);
        const int widest_predicted = (orior_exact_compare(&widest, &predicted) == 0);
        measured_widest[level] = widest;
        sim_check(results, matched == points,
                  "the scale step (the pair recursion) equals the level recursion on every point");
        sim_check(results, increasing == points, "psi is strictly increasing on every neighboring pair");
        sim_check(results, least_is_unit, "the least gap at level L is exactly gamma^-beta(L)");
        sim_check(results, widest_predicted, "the widest gap is the gap recursion's");
        mmgr_line_decimal_columns(line, level, 10u);
        mmgr_line_decimal_columns(line, points, 9u);
        mmgr_line_decimal_columns(line, matched, 19u);
        mmgr_line_decimal_columns(line, increasing, 13u);
        mmgr_line_put(line, least_is_unit ? "                      yes" : "                       NO");
        mmgr_line_put(line, widest_predicted ? "                      yes\n" : "                       NO\n");
    }
    sim_flush(results);
    platform_cell_released(below, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(table, (unsigned int)platform_place_g_program_owner);
}

void psi_pair_symbolic(const PsiCase *psi_case, const unsigned char *digit, unsigned int levels,
                       unsigned long long tilt, PsiValue *value, PsiValue *plus)
{
    static PsiValue copy;
    const unsigned long long base = psi_case->base;
    // tilt / 2 is at max (gamma - 1) / 2, far inside a word
    const SimRational carried = sim_rational((long long)tilt, 2LL);
    const SimRational half = sim_rational(1LL, 2LL);
    psi_value_zero(value);
    psi_value_zero(plus);
    psi_value_add_term(value, sim_rational((long long)digit[0], 1LL), psi_case->weight[1]);
    psi_value_add_term(plus, sim_rational((long long)digit[0] + 1LL, 1LL), psi_case->weight[1]);
    for (unsigned int level = 2u; level <= levels; level += 1u)
    {
        const unsigned long long place = digit[level - 1u];
        const unsigned long long exponent = psi_case->weight[level];
        if (place < (base - 2ULL))
        {
            *plus = *value;
            // a digit is below the base, far inside a word
            psi_value_add_term(plus, sim_rational((long long)place + 1LL, 1LL), exponent);
            psi_value_add_term(value, sim_rational((long long)place, 1LL), exponent);
        }
        else if (place == (base - 2ULL))
        {
            copy = *value;
            psi_value_scale(plus, half);
            psi_value_add(plus, &copy, half);
            psi_value_add_term(plus, carried, exponent);
            psi_value_add_term(value, sim_rational((long long)place, 1LL), exponent);
        }
        else
        {
            psi_value_scale(value, half);
            psi_value_add(value, plus, half);
            psi_value_add_term(value, carried, exponent);
        }
    }
}
