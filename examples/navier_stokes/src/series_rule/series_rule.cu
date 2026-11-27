// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// series_rule.cu: series stored by the rule of their entries, in one normal form (series_rule.h)
#include "series_rule.h"

#include "../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "record.h"

// 1 where the pool did not meet a cell of a polynomial or an expression
static int s_series_rule_unmet;

// ---- polynomials ----

void series_rule_polynomial_released(SeriesRulePolynomial *polynomial)
{
    platform_cell_released(polynomial->term, platform_place_g_program_owner);
    polynomial->term = NULL;
    polynomial->count = 0u;
    polynomial->room = 0u;
}

// `polynomial` empty, with a cell of the temporary tier of room for `room` terms
static void series_rule_polynomial_taken(SeriesRulePolynomial *polynomial, unsigned int room)
{
    polynomial->count = 0u;
    polynomial->term = (room != 0u)
                           ? (SeriesRuleTerm *)platform_cell_taken((unsigned long long)room * sizeof(SeriesRuleTerm),
                                                                   platform_place_g_program_owner, 1)
                           : NULL;
    s_series_rule_unmet |= ((room != 0u) && (polynomial->term == NULL)) ? 1 : 0;
    polynomial->room = (polynomial->term != NULL) ? room : 0u;
}

// the term of `power` and `value` put at the end of `polynomial`, which has room for it
static void series_rule_term_pushed(SeriesRulePolynomial *polynomial, const SeriesRulePower *power, SimRational value)
{
    if (polynomial->count < polynomial->room)
    {
        polynomial->term[polynomial->count].power = *power;
        polynomial->term[polynomial->count].value = value;
        polynomial->count += 1u;
    }
}

// the order of two powers, variable by variable: below 0, 0 or above 0
static int series_rule_power_order(const SeriesRulePower *left, const SeriesRulePower *right)
{
    for (unsigned int variable = 0u; variable < SERIES_RULE_VARIABLES; variable += 1u)
    {
        if (left->at[variable] != right->at[variable])
        {
            return (left->at[variable] < right->at[variable]) ? -1 : 1;
        }
    }
    return 0;
}

static int series_rule_term_order(const void *left, const void *right)
{
    return series_rule_power_order(&((const SeriesRuleTerm *)left)->power, &((const SeriesRuleTerm *)right)->power);
}

// `made`, its terms in any order, sorted once and its terms of one power added, a sum of 0 left out, into `out`
static void series_rule_polynomial_settled(SeriesRulePolynomial *made, SeriesRulePolynomial *out)
{
    if (made->count > 1u)
    {
        SeriesRuleTerm *const room = (SeriesRuleTerm *)platform_cell_taken(
            (unsigned long long)made->count * sizeof(SeriesRuleTerm), platform_place_g_program_owner, 1);
        s_series_rule_unmet |= (room == NULL) ? 1 : 0;
        if (room != NULL)
        {
            (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = made->term, .bytes = sizeof(SeriesRuleTerm),
                             .scratch = room, .count = made->count, .order = series_rule_term_order);
        }
        platform_cell_released(room, platform_place_g_program_owner);
    }
    unsigned int kept = 0u;
    unsigned int first = 0u;
    while (first < made->count)
    {
        SimRational value = made->term[first].value;
        unsigned int next = first + 1u;
        while ((next < made->count) &&
               (series_rule_power_order(&made->term[next].power, &made->term[first].power) == 0))
        {
            value = sim_rational_sum(value, made->term[next].value);
            next += 1u;
        }
        if (sim_rational_sign(value) != 0)
        {
            made->term[kept].power = made->term[first].power;
            made->term[kept].value = value;
            kept += 1u;
        }
        first = next;
    }
    made->count = kept;
    series_rule_polynomial_released(out);
    *out = *made;
    *made = SeriesRulePolynomial SERIES_RULE_NONE;
}

// a copy of `polynomial` on the temporary tier
static void series_rule_polynomial_copied(const SeriesRulePolynomial *polynomial, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, polynomial->count);
    for (unsigned int at = 0u; at < polynomial->count; at += 1u)
    {
        series_rule_term_pushed(&made, &polynomial->term[at].power, polynomial->term[at].value);
    }
    series_rule_polynomial_released(out);
    *out = made;
}

void series_rule_constant(SimRational value, SeriesRulePolynomial *out)
{
    const SeriesRulePower power = {{0u, 0u, 0u, 0u}};
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, 1u);
    if (sim_rational_sign(value) != 0)
    {
        series_rule_term_pushed(&made, &power, value);
    }
    series_rule_polynomial_released(out);
    *out = made;
}

void series_rule_variable(SeriesRuleVariable variable, SeriesRulePolynomial *out)
{
    SeriesRulePower power = {{0u, 0u, 0u, 0u}};
    power.at[variable] = 1u;
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, 1u);
    series_rule_term_pushed(&made, &power, sim_rational(1ll, 1ll));
    series_rule_polynomial_released(out);
    *out = made;
}

void series_rule_polynomial_sum(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                SeriesRulePolynomial *out)
{
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, left->count + right->count);
    for (unsigned int at = 0u; at < left->count; at += 1u)
    {
        series_rule_term_pushed(&made, &left->term[at].power, left->term[at].value);
    }
    for (unsigned int at = 0u; at < right->count; at += 1u)
    {
        series_rule_term_pushed(&made, &right->term[at].power, right->term[at].value);
    }
    series_rule_polynomial_settled(&made, out);
}

static void series_rule_polynomial_scaled(const SeriesRulePolynomial *polynomial, SimRational factor,
                                          SeriesRulePolynomial *out)
{
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, polynomial->count);
    for (unsigned int at = 0u; at < polynomial->count; at += 1u)
    {
        series_rule_term_pushed(&made, &polynomial->term[at].power,
                                sim_rational_product(polynomial->term[at].value, factor));
    }
    series_rule_polynomial_settled(&made, out);
}

void series_rule_polynomial_difference(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                       SeriesRulePolynomial *out)
{
    SeriesRulePolynomial turned = SERIES_RULE_NONE;
    series_rule_polynomial_scaled(right, sim_rational(-1ll, 1ll), &turned);
    series_rule_polynomial_sum(left, &turned, out);
    series_rule_polynomial_released(&turned);
}

void series_rule_polynomial_product(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                    SeriesRulePolynomial *out)
{
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, left->count * right->count);
    for (unsigned int one = 0u; one < left->count; one += 1u)
    {
        for (unsigned int other = 0u; other < right->count; other += 1u)
        {
            SeriesRulePower power;
            for (unsigned int variable = 0u; variable < SERIES_RULE_VARIABLES; variable += 1u)
            {
                power.at[variable] = left->term[one].power.at[variable] + right->term[other].power.at[variable];
            }
            series_rule_term_pushed(&made, &power,
                                    sim_rational_product(left->term[one].value, right->term[other].value));
        }
    }
    series_rule_polynomial_settled(&made, out);
}

// the polynomial with `variable` replaced by `value`
static void series_rule_substitute(const SeriesRulePolynomial *polynomial, SeriesRuleVariable variable,
                                   const SeriesRulePolynomial *value, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial result = SERIES_RULE_NONE;
    SeriesRulePolynomial part = SERIES_RULE_NONE;
    for (unsigned int at = 0u; at < polynomial->count; at += 1u)
    {
        SeriesRulePower rest = polynomial->term[at].power;
        rest.at[variable] = 0u;
        series_rule_polynomial_released(&part);
        series_rule_polynomial_taken(&part, 1u);
        series_rule_term_pushed(&part, &rest, polynomial->term[at].value);
        for (unsigned int power = 0u; power < polynomial->term[at].power.at[variable]; power += 1u)
        {
            series_rule_polynomial_product(&part, value, &part);
        }
        series_rule_polynomial_sum(&result, &part, &result);
    }
    series_rule_polynomial_released(&part);
    series_rule_polynomial_released(out);
    *out = result;
}

// k + shift, or i + shift, as a polynomial
static void series_rule_moved(SeriesRuleVariable variable, long long shift, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial alone = SERIES_RULE_NONE;
    SeriesRulePolynomial constant = SERIES_RULE_NONE;
    series_rule_variable(variable, &alone);
    series_rule_constant(sim_rational(shift, 1ll), &constant);
    series_rule_polynomial_sum(&alone, &constant, out);
    series_rule_polynomial_released(&alone);
    series_rule_polynomial_released(&constant);
}

// L = 1 - 2 h eta^2
static void series_rule_l(SeriesRulePolynomial *out)
{
    const SeriesRulePower one = {{0u, 0u, 0u, 0u}};
    const SeriesRulePower eta_square_h = {{2u, 1u, 0u, 0u}};
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, 2u);
    series_rule_term_pushed(&made, &one, sim_rational(1ll, 1ll));
    series_rule_term_pushed(&made, &eta_square_h, sim_rational(-2ll, 1ll));
    series_rule_polynomial_settled(&made, out);
}

// the polynomial times L^count
static void series_rule_raised(const SeriesRulePolynomial *polynomial, unsigned int count, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial l = SERIES_RULE_NONE;
    SeriesRulePolynomial made = SERIES_RULE_NONE;
    series_rule_l(&l);
    series_rule_polynomial_copied(polynomial, &made);
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        series_rule_polynomial_product(&made, &l, &made);
    }
    series_rule_polynomial_released(&l);
    series_rule_polynomial_released(out);
    *out = made;
}

// d/deta of the polynomial
static void series_rule_polynomial_slope(const SeriesRulePolynomial *polynomial, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial made;
    series_rule_polynomial_taken(&made, polynomial->count);
    for (unsigned int at = 0u; at < polynomial->count; at += 1u)
    {
        const SeriesRuleTerm *const term = &polynomial->term[at];
        if (term->power.at[SERIES_RULE_ETA] == 0u)
        {
            continue;
        }
        SeriesRulePower power = term->power;
        power.at[SERIES_RULE_ETA] -= 1u;
        series_rule_term_pushed(
            &made, &power,
            sim_rational_product(term->value, sim_rational((long long)term->power.at[SERIES_RULE_ETA], 1ll)));
    }
    series_rule_polynomial_settled(&made, out);
}

// ---- expressions ----

void series_rule_released(SeriesRule *rule)
{
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        series_rule_polynomial_released(&rule->part[at].factor.numerator);
    }
    platform_cell_released(rule->part, platform_place_g_program_owner);
    rule->part = NULL;
    rule->count = 0u;
    rule->room = 0u;
}

// `rule` empty, with a cell of the temporary tier of room for `room` parts
static void series_rule_taken(SeriesRule *rule, unsigned int room)
{
    rule->count = 0u;
    rule->part = (room != 0u) ? (SeriesRulePart *)platform_cell_cleared(room, sizeof(SeriesRulePart),
                                                                        platform_place_g_program_owner, 1)
                              : NULL;
    s_series_rule_unmet |= ((room != 0u) && (rule->part == NULL)) ? 1 : 0;
    rule->room = (rule->part != NULL) ? room : 0u;
}

static int series_rule_key_order(const SeriesRuleKey *left, const SeriesRuleKey *right)
{
    const long long first[7] = {left->product,      left->first.field,  left->first.shift, left->first.slope,
                                left->second.field, left->second.shift, left->second.slope};
    const long long second[7] = {right->product,      right->first.field,  right->first.shift, right->first.slope,
                                 right->second.field, right->second.shift, right->second.slope};
    for (unsigned int index = 0u; index < 7u; index += 1u)
    {
        if (first[index] != second[index])
        {
            return (first[index] < second[index]) ? -1 : 1;
        }
    }
    return 0;
}

static int series_rule_part_order(const void *left, const void *right)
{
    return series_rule_key_order(&((const SeriesRulePart *)left)->key, &((const SeriesRulePart *)right)->key);
}

// The part of `key` and a copy of `numerator` over L^power put at the end of `rule`, which has room for it. Where
// `normal` is 1 the part takes the normal form first: a Cauchy product's pair stored in its order, and a pair of one
// entry stored even under i -> k + c - i
static void series_rule_part_pushed(SeriesRule *rule, SeriesRuleKey key, const SeriesRulePolynomial *numerator,
                                    unsigned int power, int normal)
{
    if (rule->count >= rule->room)
    {
        s_series_rule_unmet = 1;
        return;
    }
    SeriesRulePolynomial made = SERIES_RULE_NONE;
    series_rule_polynomial_copied(numerator, &made);
    if (normal && key.product)
    {
        // i -> k + c - i
        SeriesRulePolynomial moved = SERIES_RULE_NONE;
        SeriesRulePolynomial alone = SERIES_RULE_NONE;
        SeriesRulePolynomial turned = SERIES_RULE_NONE;
        series_rule_moved(SERIES_RULE_K, key.second.shift, &moved);
        series_rule_variable(SERIES_RULE_I, &alone);
        series_rule_polynomial_difference(&moved, &alone, &turned);
        const long long first[2] = {key.first.field, key.first.slope};
        const long long second[2] = {key.second.field, key.second.slope};
        if ((first[0] > second[0]) || ((first[0] == second[0]) && (first[1] > second[1])))
        {
            const unsigned int field = key.first.field;
            const unsigned int slope = key.first.slope;
            key.first.field = key.second.field;
            key.first.slope = key.second.slope;
            key.second.field = field;
            key.second.slope = slope;
            series_rule_substitute(&made, SERIES_RULE_I, &turned, &made);
        }
        else if ((first[0] == second[0]) && (first[1] == second[1]))
        {
            SeriesRulePolynomial mirrored = SERIES_RULE_NONE;
            series_rule_substitute(&made, SERIES_RULE_I, &turned, &mirrored);
            series_rule_polynomial_sum(&made, &mirrored, &made);
            series_rule_polynomial_scaled(&made, sim_rational(1ll, 2ll), &made);
            series_rule_polynomial_released(&mirrored);
        }
        series_rule_polynomial_released(&moved);
        series_rule_polynomial_released(&alone);
        series_rule_polynomial_released(&turned);
    }
    rule->part[rule->count].key = key;
    rule->part[rule->count].factor.numerator = made;
    rule->part[rule->count].factor.power = power;
    rule->count += 1u;
}

// `made`, its parts in any order, sorted once by their keys into `out`. The parts of one key add in the order they
// came: an empty part adds nothing, the first one that is not empty stands, and each after it adds over the higher
// power of L, a sum of 0 leaving no part until the next one
static void series_rule_settled(SeriesRule *made, SeriesRule *out)
{
    if (made->count > 1u)
    {
        SeriesRulePart *const room = (SeriesRulePart *)platform_cell_taken(
            (unsigned long long)made->count * sizeof(SeriesRulePart), platform_place_g_program_owner, 1);
        s_series_rule_unmet |= (room == NULL) ? 1 : 0;
        if (room != NULL)
        {
            (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = made->part, .bytes = sizeof(SeriesRulePart),
                             .scratch = room, .count = made->count, .order = series_rule_part_order);
        }
        platform_cell_released(room, platform_place_g_program_owner);
    }
    unsigned int kept = 0u;
    unsigned int first = 0u;
    while (first < made->count)
    {
        unsigned int next = first;
        int standing = 0;
        SeriesRulePart current;
        while ((next < made->count) && (series_rule_key_order(&made->part[next].key, &made->part[first].key) == 0))
        {
            SeriesRulePart *const part = &made->part[next];
            next += 1u;
            if (part->factor.numerator.count == 0u)
            {
                series_rule_polynomial_released(&part->factor.numerator);
                continue;
            }
            if (!standing)
            {
                current = *part;
                part->factor.numerator = SeriesRulePolynomial SERIES_RULE_NONE;
                standing = 1;
                continue;
            }
            const unsigned int power =
                (part->factor.power > current.factor.power) ? part->factor.power : current.factor.power;
            SeriesRulePolynomial raised_current = SERIES_RULE_NONE;
            SeriesRulePolynomial raised_part = SERIES_RULE_NONE;
            series_rule_raised(&current.factor.numerator, power - current.factor.power, &raised_current);
            series_rule_raised(&part->factor.numerator, power - part->factor.power, &raised_part);
            series_rule_polynomial_released(&current.factor.numerator);
            series_rule_polynomial_released(&part->factor.numerator);
            series_rule_polynomial_sum(&raised_current, &raised_part, &current.factor.numerator);
            series_rule_polynomial_released(&raised_current);
            series_rule_polynomial_released(&raised_part);
            current.factor.power = power;
            if (current.factor.numerator.count == 0u)
            {
                series_rule_polynomial_released(&current.factor.numerator);
                standing = 0;
            }
        }
        if (standing)
        {
            made->part[kept] = current;
            kept += 1u;
        }
        first = next;
    }
    made->count = kept;
    series_rule_released(out);
    *out = *made;
    *made = SeriesRule SERIES_RULE_NONE;
}

void series_rule_part(const SeriesRulePolynomial *factor, unsigned int power, unsigned int field, int shift,
                      unsigned int slope, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, 1u);
    const SeriesRuleKey key = {0, {field, shift, slope}, {0u, 0, 0u}};
    series_rule_part_pushed(&made, key, factor, power, 1);
    series_rule_settled(&made, out);
}

void series_rule_entry(unsigned int field, SeriesRule *out)
{
    SeriesRulePolynomial one = SERIES_RULE_NONE;
    series_rule_constant(sim_rational(1ll, 1ll), &one);
    series_rule_part(&one, 0u, field, 0, 0u, out);
    series_rule_polynomial_released(&one);
}

void series_rule_cauchy(const SeriesRulePolynomial *factor, unsigned int power, unsigned int first,
                        unsigned int first_slope, unsigned int second, unsigned int second_slope, int shift,
                        SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, 1u);
    const SeriesRuleKey key = {1, {first, 0, first_slope}, {second, shift, second_slope}};
    series_rule_part_pushed(&made, key, factor, power, 1);
    series_rule_settled(&made, out);
}

void series_rule_sum(const SeriesRule *left, const SeriesRule *right, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, left->count + right->count);
    for (unsigned int at = 0u; at < left->count; at += 1u)
    {
        series_rule_part_pushed(&made, left->part[at].key, &left->part[at].factor.numerator,
                                left->part[at].factor.power, 0);
    }
    for (unsigned int at = 0u; at < right->count; at += 1u)
    {
        series_rule_part_pushed(&made, right->part[at].key, &right->part[at].factor.numerator,
                                right->part[at].factor.power, 1);
    }
    series_rule_settled(&made, out);
}

void series_rule_difference(const SeriesRule *left, const SeriesRule *right, SeriesRule *out)
{
    SeriesRulePolynomial minus = SERIES_RULE_NONE;
    SeriesRule turned = SERIES_RULE_NONE;
    series_rule_constant(sim_rational(-1ll, 1ll), &minus);
    series_rule_scaled(right, &minus, 0u, &turned);
    series_rule_sum(left, &turned, out);
    series_rule_polynomial_released(&minus);
    series_rule_released(&turned);
}

void series_rule_scaled(const SeriesRule *rule, const SeriesRulePolynomial *factor, unsigned int power, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, rule->count);
    SeriesRulePolynomial product = SERIES_RULE_NONE;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        series_rule_polynomial_product(&rule->part[at].factor.numerator, factor, &product);
        series_rule_part_pushed(&made, rule->part[at].key, &product, rule->part[at].factor.power + power, 1);
    }
    series_rule_polynomial_released(&product);
    series_rule_settled(&made, out);
}

void series_rule_shift(const SeriesRule *rule, int shift, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, rule->count);
    SeriesRulePolynomial value = SERIES_RULE_NONE;
    SeriesRulePolynomial moved = SERIES_RULE_NONE;
    series_rule_moved(SERIES_RULE_K, shift, &value);
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        SeriesRuleKey key = rule->part[at].key;
        if (key.product)
        {
            key.second.shift += shift;
        }
        else
        {
            key.first.shift += shift;
        }
        series_rule_substitute(&rule->part[at].factor.numerator, SERIES_RULE_K, &value, &moved);
        series_rule_part_pushed(&made, key, &moved, rule->part[at].factor.power, 1);
    }
    series_rule_polynomial_released(&value);
    series_rule_polynomial_released(&moved);
    series_rule_settled(&made, out);
}

// (X f)_k = f_(k-1)
void series_rule_times_x(const SeriesRule *rule, SeriesRule *out)
{
    series_rule_shift(rule, -1, out);
}

// (f_X)_k = (k + 1) f_(k+1)
void series_rule_slope_x(const SeriesRule *rule, SeriesRule *out)
{
    SeriesRule shifted = SERIES_RULE_NONE;
    SeriesRulePolynomial next = SERIES_RULE_NONE;
    series_rule_shift(rule, 1, &shifted);
    series_rule_moved(SERIES_RULE_K, 1ll, &next);
    series_rule_scaled(&shifted, &next, 0u, out);
    series_rule_released(&shifted);
    series_rule_polynomial_released(&next);
}

// (X f_X)_k = k f_k
void series_rule_euler(const SeriesRule *rule, SeriesRule *out)
{
    SeriesRulePolynomial k = SERIES_RULE_NONE;
    series_rule_variable(SERIES_RULE_K, &k);
    series_rule_scaled(rule, &k, 0u, out);
    series_rule_polynomial_released(&k);
}

// (f g)_k = sum_i f_i g_(k-i); with f_i = a(i) A_(i+s) and g_(k-i) = b(k-i) B_(k-i+t), j = i + s gives
// sum_j a(j - s) b(k - j + s) A_j B_(k+s+t-j)
void series_rule_product(const SeriesRule *left, const SeriesRule *right, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, left->count * right->count);
    SeriesRulePolynomial at_i = SERIES_RULE_NONE;
    SeriesRulePolynomial moved_k = SERIES_RULE_NONE;
    SeriesRulePolynomial alone_i = SERIES_RULE_NONE;
    SeriesRulePolynomial at_rest = SERIES_RULE_NONE;
    SeriesRulePolynomial at_first = SERIES_RULE_NONE;
    SeriesRulePolynomial at_second = SERIES_RULE_NONE;
    SeriesRulePolynomial both = SERIES_RULE_NONE;
    series_rule_variable(SERIES_RULE_I, &alone_i);
    for (unsigned int one_at = 0u; one_at < left->count; one_at += 1u)
    {
        for (unsigned int other_at = 0u; other_at < right->count; other_at += 1u)
        {
            const SeriesRulePart *const first = &left->part[one_at];
            const SeriesRulePart *const second = &right->part[other_at];
            const SeriesRuleEntry &one = first->key.first;
            const SeriesRuleEntry &other = second->key.first;
            series_rule_moved(SERIES_RULE_I, -(long long)one.shift, &at_i);
            series_rule_substitute(&first->factor.numerator, SERIES_RULE_K, &at_i, &at_first);
            series_rule_moved(SERIES_RULE_K, one.shift, &moved_k);
            series_rule_polynomial_difference(&moved_k, &alone_i, &at_rest);
            series_rule_substitute(&second->factor.numerator, SERIES_RULE_K, &at_rest, &at_second);
            series_rule_polynomial_product(&at_first, &at_second, &both);
            const SeriesRuleKey key = {1, {one.field, 0, one.slope}, {other.field, one.shift + other.shift, other.slope}};
            series_rule_part_pushed(&made, key, &both, first->factor.power + second->factor.power, 1);
        }
    }
    SeriesRulePolynomial *const done[7] = {&at_i, &moved_k, &alone_i, &at_rest, &at_first, &at_second, &both};
    for (unsigned int each = 0u; each < 7u; each += 1u)
    {
        series_rule_polynomial_released(done[each]);
    }
    series_rule_settled(&made, out);
}

// d/deta (N L^-n) = (N' L + 4 n h eta N) L^-(n+1)
void series_rule_slope_eta(const SeriesRule *rule, SeriesRule *out)
{
    SeriesRule made;
    series_rule_taken(&made, 3u * rule->count);
    const SeriesRulePower h_eta_power = {{1u, 1u, 0u, 0u}};
    SeriesRulePolynomial h_eta = SERIES_RULE_NONE;
    series_rule_polynomial_taken(&h_eta, 1u);
    series_rule_term_pushed(&h_eta, &h_eta_power, sim_rational(4ll, 1ll));
    SeriesRulePolynomial l = SERIES_RULE_NONE;
    series_rule_l(&l);
    SeriesRulePolynomial slope = SERIES_RULE_NONE;
    SeriesRulePolynomial slope_l = SERIES_RULE_NONE;
    SeriesRulePolynomial h_eta_n = SERIES_RULE_NONE;
    SeriesRulePolynomial h_eta_part = SERIES_RULE_NONE;
    SeriesRulePolynomial turned = SERIES_RULE_NONE;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        const SeriesRulePart *const part = &rule->part[at];
        const SeriesRuleFactor &factor = part->factor;
        series_rule_polynomial_slope(&factor.numerator, &slope);
        if (factor.power == 0u)
        {
            series_rule_part_pushed(&made, part->key, &slope, 0u, 1);
        }
        else
        {
            series_rule_polynomial_product(&slope, &l, &slope_l);
            series_rule_polynomial_scaled(&h_eta, sim_rational((long long)factor.power, 1ll), &h_eta_n);
            series_rule_polynomial_product(&h_eta_n, &factor.numerator, &h_eta_part);
            series_rule_polynomial_sum(&slope_l, &h_eta_part, &turned);
            series_rule_part_pushed(&made, part->key, &turned, factor.power + 1u, 1);
        }
        SeriesRuleKey key = part->key;
        key.first.slope += 1u;
        series_rule_part_pushed(&made, key, &factor.numerator, factor.power, 1);
        if (part->key.product)
        {
            key = part->key;
            key.second.slope += 1u;
            series_rule_part_pushed(&made, key, &factor.numerator, factor.power, 1);
        }
    }
    SeriesRulePolynomial *const done[7] = {&h_eta, &l, &slope, &slope_l, &h_eta_n, &h_eta_part, &turned};
    for (unsigned int each = 0u; each < 7u; each += 1u)
    {
        series_rule_polynomial_released(done[each]);
    }
    series_rule_settled(&made, out);
}

int series_rule_zero(const SeriesRule *rule)
{
    return rule->count == 0u;
}

// the parity of one entry: its field's, turned by every derivative in eta
static int series_rule_entry_parity(const SeriesRuleEntry *entry, const int *parities)
{
    return ((entry->slope % 2u) == 0u) ? parities[entry->field] : -parities[entry->field];
}

int series_rule_parity(const SeriesRule *rule, const int *parities)
{
    int matched = 0;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        const SeriesRulePart *const part = &rule->part[at];
        int factor = 0;
        for (unsigned int term = 0u; term < part->factor.numerator.count; term += 1u)
        {
            const int parity = ((part->factor.numerator.term[term].power.at[SERIES_RULE_ETA] % 2u) == 0u) ? 1 : -1;
            if ((factor != 0) && (factor != parity))
            {
                return 0;
            }
            factor = parity;
        }
        int parity = factor * series_rule_entry_parity(&part->key.first, parities);
        if (part->key.product)
        {
            parity *= series_rule_entry_parity(&part->key.second, parities);
        }
        if ((matched != 0) && (matched != parity))
        {
            return 0;
        }
        matched = parity;
    }
    return matched;
}

static void series_rule_polynomial_text(const SeriesRulePolynomial *polynomial, VerbaLinea *line)
{
    static const char *const names[SERIES_RULE_VARIABLES] = {"eta", "h", "k", "i"};
    mmgr_line_ch(line, '(');
    for (unsigned int at = 0u; at < polynomial->count; at += 1u)
    {
        mmgr_line_put(line, (at == 0u) ? "" : " + ");
        record_rational(line, polynomial->term[at].value);
        for (unsigned int variable = 0u; variable < SERIES_RULE_VARIABLES; variable += 1u)
        {
            const unsigned int power = polynomial->term[at].power.at[variable];
            if (power == 0u)
            {
                continue;
            }
            mmgr_line_ch(line, ' ');
            mmgr_line_put(line, names[variable]);
            if (power > 1u)
            {
                mmgr_line_ch(line, '^');
                mmgr_line_decimal(line, power, 1u);
            }
        }
    }
    mmgr_line_ch(line, ')');
}

static void series_rule_entry_text(const SeriesRuleEntry *entry, const char *at, const char *const *names,
                                   VerbaLinea *line)
{
    mmgr_line_put(line, names[entry->field]);
    for (unsigned int slope = 0u; slope < entry->slope; slope += 1u)
    {
        mmgr_line_ch(line, '\'');
    }
    mmgr_line_ch(line, '[');
    mmgr_line_put(line, at);
    if (entry->shift != 0)
    {
        mmgr_line_put(line, (entry->shift > 0) ? " + " : " - ");
        mmgr_line_decimal(line, (unsigned long long)((entry->shift > 0) ? entry->shift : -entry->shift), 1u);
    }
    mmgr_line_ch(line, ']');
}

void series_rule_text(const SeriesRule *rule, const char *const *names, VerbaLinea *line)
{
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        const SeriesRulePart *const part = &rule->part[at];
        series_rule_polynomial_text(&part->factor.numerator, line);
        if (part->factor.power > 0u)
        {
            mmgr_line_put(line, " L^-");
            mmgr_line_decimal(line, part->factor.power, 1u);
        }
        if (part->key.product)
        {
            mmgr_line_put(line, " sum_i ");
            series_rule_entry_text(&part->key.first, "i", names, line);
            mmgr_line_ch(line, ' ');
            series_rule_entry_text(&part->key.second, "k - i", names, line);
        }
        else
        {
            mmgr_line_ch(line, ' ');
            series_rule_entry_text(&part->key.first, "k", names, line);
        }
        mmgr_line_ch(line, '\n');
    }
    if (rule->count == 0u)
    {
        mmgr_line_put(line, "0\n");
    }
}

unsigned int series_rule_parts(const SeriesRule *rule)
{
    return rule->count;
}

int series_rule_short(void)
{
    return (g_sim_rational_wide != 0) || s_series_rule_unmet;
}
