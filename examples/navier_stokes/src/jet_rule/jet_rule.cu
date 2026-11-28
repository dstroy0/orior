// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jet_rule.cu: stored functions and their relations, in one normal form (jet_rule.h)
#include "jet_rule.h"

#include "../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "record.h"

// the places of the factors: the fixed ones, then constants and the derivatives of stored functions as they are named
enum
{
    JET_RULE_ETA = 0u,
    JET_RULE_ANISOTROPY = 1u,
    JET_RULE_L = 2u,
    JET_RULE_X = 3u,
    JET_RULE_D = 4u,
    JET_RULE_TWO = 5u,
    JET_RULE_W = 8u,
    JET_RULE_NAMED = 16u
};

// the bytes of a name, its terminator among them, and the places and functions the tables hold at max
#define JET_RULE_NAME 64u
#define JET_RULE_PLACES 256u
#define JET_RULE_FUNCTIONS 64u

// the terms an added term spreads to at max as the rule writes eta^2 as 1 - d, which the room of a result counts
#define JET_RULE_SPREAD 4u

// a named place: a constant, or a derivative of a stored function
typedef struct
{
    char name[JET_RULE_NAME];
    int constant;
    unsigned int function;
    unsigned int a;
    unsigned int b;
} JetRulePlace;

// a stored function: its name, and the relation of its derivative of order `order` in X where it has one
typedef struct
{
    char name[JET_RULE_NAME];
    int related;
    unsigned int order;
    JetRule value;
} JetRuleFunction;

// The named places and the stored functions, each a table at a bound with a count, in the order of their names
static JetRulePlace s_jet_rule_places[JET_RULE_PLACES];
static unsigned int s_jet_rule_place_count;
static JetRuleFunction s_jet_rule_functions[JET_RULE_FUNCTIONS];
static unsigned int s_jet_rule_function_count;
static int s_jet_rule_wide = 0;

// an exponent past this is taken as past a word
static const long long s_jet_rule_exponent_max = 1ll << 40;

// the factors of a product before it is tidy: two products' and the factors that the rewrite of eta^2 puts after them
typedef struct
{
    JetRuleFactor factor[(2u * JET_RULE_FACTORS) + 4u];
    unsigned int count;
} JetRuleWork;

static long long jet_rule_gcd(long long left, long long right)
{
    left = (left < 0ll) ? -left : left;
    right = (right < 0ll) ? -right : right;
    while (right != 0ll)
    {
        const long long rest = left % right;
        left = right;
        right = rest;
    }
    return (left == 0ll) ? 1ll : left;
}

// numerator / denominator in lowest terms, the denominator positive
static void jet_rule_settle(long long *numerator, long long *denominator)
{
    if (*denominator < 0ll)
    {
        *numerator = -*numerator;
        *denominator = -*denominator;
    }
    const long long common = jet_rule_gcd(*numerator, *denominator);
    *numerator /= common;
    *denominator /= common;
    if ((*numerator > s_jet_rule_exponent_max) || (*numerator < -s_jet_rule_exponent_max) ||
        (*denominator > s_jet_rule_exponent_max))
    {
        s_jet_rule_wide = 1;
    }
}

static void jet_rule_add_exponent(long long *numerator, long long *denominator, long long other_numerator,
                                  long long other_denominator)
{
    *numerator = *numerator * other_denominator + other_numerator * *denominator;
    *denominator = *denominator * other_denominator;
    jet_rule_settle(numerator, denominator);
}

// the order of two factors, field by field: below 0, 0 or above 0
static int jet_rule_factor_order(const JetRuleFactor *one, const JetRuleFactor *other)
{
    const long long first[5] = {one->place, one->a_numerator, one->a_denominator, one->b_numerator, one->b_denominator};
    const long long second[5] = {other->place, other->a_numerator, other->a_denominator, other->b_numerator,
                                 other->b_denominator};
    for (unsigned int part = 0u; part < 5u; part += 1u)
    {
        if (first[part] != second[part])
        {
            return (first[part] < second[part]) ? -1 : 1;
        }
    }
    return 0;
}

// the order of two products: factor by factor over the shorter, then the shorter first
static int jet_rule_monomial_order(const JetRuleMonomial *left, const JetRuleMonomial *right)
{
    const unsigned int size = (left->count < right->count) ? left->count : right->count;
    for (unsigned int index = 0u; index < size; index += 1u)
    {
        const int order = jet_rule_factor_order(&left->factor[index], &right->factor[index]);
        if (order != 0)
        {
            return order;
        }
    }
    return (left->count < right->count) ? -1 : ((left->count > right->count) ? 1 : 0);
}

static int jet_rule_term_order(const void *left, const void *right)
{
    return jet_rule_monomial_order(&((const JetRuleTerm *)left)->monomial, &((const JetRuleTerm *)right)->monomial);
}

static int jet_rule_place_order(const void *left, const void *right)
{
    const unsigned int one = ((const JetRuleFactor *)left)->place;
    const unsigned int other = ((const JetRuleFactor *)right)->place;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

static JetRuleFactor jet_rule_factor(unsigned int place, long long a_numerator, long long a_denominator,
                                     long long b_numerator, long long b_denominator)
{
    JetRuleFactor factor = {place, a_numerator, a_denominator, b_numerator, b_denominator};
    jet_rule_settle(&factor.a_numerator, &factor.a_denominator);
    jet_rule_settle(&factor.b_numerator, &factor.b_denominator);
    return factor;
}

// `factor` put at the end of `work`, which has room for it
static void jet_rule_work_pushed(JetRuleWork *work, JetRuleFactor factor)
{
    if (work->count >= (sizeof(work->factor) / sizeof(work->factor[0])))
    {
        s_jet_rule_wide = 1;
        return;
    }
    work->factor[work->count] = factor;
    work->count += 1u;
}

// `monomial` as the factors of a work product
static void jet_rule_work_of(const JetRuleMonomial *monomial, JetRuleWork *work)
{
    work->count = 0u;
    for (unsigned int index = 0u; index < monomial->count; index += 1u)
    {
        jet_rule_work_pushed(work, monomial->factor[index]);
    }
}

// the work's factors at one place each, in the order of their places. The exponents of each place add in the order
// they came, and exponent 0 goes out. Whole powers of 2 move into `coefficient`
static void jet_rule_tidy(JetRuleWork *work, JetRuleMonomial *tidy, SimRational *coefficient)
{
    JetRuleFactor room[(2u * JET_RULE_FACTORS) + 4u];
    if (work->count > 1u)
    {
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = work->factor, .bytes = sizeof(JetRuleFactor), .scratch = room,
                         .count = work->count, .order = jet_rule_place_order);
    }
    tidy->count = 0u;
    unsigned int first = 0u;
    while (first < work->count)
    {
        JetRuleFactor factor = work->factor[first];
        unsigned int next = first + 1u;
        while ((next < work->count) && (work->factor[next].place == factor.place))
        {
            jet_rule_add_exponent(&factor.a_numerator, &factor.a_denominator, work->factor[next].a_numerator,
                                  work->factor[next].a_denominator);
            jet_rule_add_exponent(&factor.b_numerator, &factor.b_denominator, work->factor[next].b_numerator,
                                  work->factor[next].b_denominator);
            next += 1u;
        }
        first = next;
        if (factor.place == JET_RULE_TWO)
        {
            // the whole part of a, floor, into the coefficient
            long long whole = factor.a_numerator / factor.a_denominator;
            if ((factor.a_numerator % factor.a_denominator != 0ll) && (factor.a_numerator < 0ll))
            {
                whole -= 1ll;
            }
            factor.a_numerator -= whole * factor.a_denominator;
            const long long size = (whole < 0ll) ? -whole : whole;
            if (size > 4096ll)
            {
                s_jet_rule_wide = 1;
            }
            for (long long step = 0ll; (step < size) && (step <= 4096ll); step += 1ll)
            {
                *coefficient = sim_rational_product(*coefficient, (whole > 0ll) ? sim_rational(2ll, 1ll) : sim_rational(1ll, 2ll));
            }
        }
        if ((factor.a_numerator != 0ll) || (factor.b_numerator != 0ll))
        {
            if (tidy->count >= JET_RULE_FACTORS)
            {
                s_jet_rule_wide = 1;
                continue;
            }
            tidy->factor[tidy->count] = factor;
            tidy->count += 1u;
        }
    }
}

// the exponent of eta in the product
static long long jet_rule_eta_power(const JetRuleMonomial *monomial)
{
    for (unsigned int index = 0u; index < monomial->count; index += 1u)
    {
        if (monomial->factor[index].place == JET_RULE_ETA)
        {
            return monomial->factor[index].a_numerator;
        }
    }
    return 0ll;
}

void jet_rule_released(JetRule *rule)
{
    platform_cell_released(rule->term, platform_place_g_program_owner);
    rule->term = NULL;
    rule->count = 0u;
    rule->room = 0u;
}

// `rule` empty, with a cell of the temporary tier of room for `room` terms, or of the persistent tier where `ephemeral`
// is 0
static void jet_rule_taken(JetRule *rule, unsigned int room, int ephemeral)
{
    rule->count = 0u;
    rule->term = (room != 0u) ? (JetRuleTerm *)platform_cell_taken((unsigned long long)room * sizeof(JetRuleTerm),
                                                                   platform_place_g_program_owner, ephemeral)
                              : NULL;
    s_jet_rule_wide |= ((room != 0u) && (rule->term == NULL)) ? 1 : 0;
    rule->room = (rule->term != NULL) ? room : 0u;
}

// the term of `monomial` and `value` put at the end of `rule`, which has room for it
static void jet_rule_term_pushed(JetRule *rule, const JetRuleMonomial *monomial, SimRational value)
{
    if (rule->count >= rule->room)
    {
        s_jet_rule_wide = 1;
        return;
    }
    rule->term[rule->count].monomial = *monomial;
    rule->term[rule->count].value = value;
    rule->count += 1u;
}

// the term of the work product and `coefficient` added to `made`, tidy, eta^2 written as 1 - d
static void jet_rule_add(JetRule *made, JetRuleWork *work, SimRational coefficient)
{
    if (sim_rational_sign(coefficient) == 0)
    {
        return;
    }
    JetRuleMonomial tidy;
    jet_rule_tidy(work, &tidy, &coefficient);
    if (jet_rule_eta_power(&tidy) >= 2ll)
    {
        JetRuleWork lower;
        jet_rule_work_of(&tidy, &lower);
        jet_rule_work_pushed(&lower, jet_rule_factor(JET_RULE_ETA, -2ll, 1ll, 0ll, 1ll));
        JetRuleWork again = lower;
        jet_rule_add(made, &again, coefficient);
        jet_rule_work_pushed(&lower, jet_rule_factor(JET_RULE_D, 1ll, 1ll, 0ll, 1ll));
        jet_rule_add(made, &lower, sim_rational_negative(coefficient));
        return;
    }
    jet_rule_term_pushed(made, &tidy, coefficient);
}

// `made`, its terms in any order, sorted once and the terms of one product added, a sum of 0 left out, into `out`
static void jet_rule_settled(JetRule *made, JetRule *out)
{
    if (made->count > 1u)
    {
        JetRuleTerm *const room = (JetRuleTerm *)platform_cell_taken(
            (unsigned long long)made->count * sizeof(JetRuleTerm), platform_place_g_program_owner, 1);
        s_jet_rule_wide |= (room == NULL) ? 1 : 0;
        if (room != NULL)
        {
            (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = made->term, .bytes = sizeof(JetRuleTerm), .scratch = room,
                             .count = made->count, .order = jet_rule_term_order);
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
               (jet_rule_monomial_order(&made->term[next].monomial, &made->term[first].monomial) == 0))
        {
            value = sim_rational_sum(value, made->term[next].value);
            next += 1u;
        }
        if (sim_rational_sign(value) != 0)
        {
            if (kept != first)
            {
                made->term[kept].monomial = made->term[first].monomial;
            }
            made->term[kept].value = value;
            kept += 1u;
        }
        first = next;
    }
    made->count = kept;
    jet_rule_released(out);
    *out = *made;
    *made = JetRule JET_RULE_NONE;
}

// the expression of the one factor `factor`
static void jet_rule_single(JetRuleFactor factor, JetRule *out)
{
    JetRule made;
    jet_rule_taken(&made, JET_RULE_SPREAD, 1);
    JetRuleWork work;
    work.count = 0u;
    jet_rule_work_pushed(&work, factor);
    jet_rule_add(&made, &work, sim_rational(1ll, 1ll));
    jet_rule_settled(&made, out);
}

void jet_rule_rational(SimRational value, JetRule *out)
{
    JetRule made;
    jet_rule_taken(&made, 1u, 1);
    JetRuleWork work;
    work.count = 0u;
    jet_rule_add(&made, &work, value);
    jet_rule_settled(&made, out);
}

void jet_rule_number(long long numerator, long long denominator, JetRule *out)
{
    jet_rule_rational(sim_rational(numerator, denominator), out);
}

void jet_rule_eta(JetRule *out)
{
    jet_rule_single(jet_rule_factor(JET_RULE_ETA, 1ll, 1ll, 0ll, 1ll), out);
}

void jet_rule_h(JetRule *out)
{
    jet_rule_single(jet_rule_factor(JET_RULE_ANISOTROPY, 1ll, 1ll, 0ll, 1ll), out);
}

void jet_rule_over_l(JetRule *out)
{
    jet_rule_single(jet_rule_factor(JET_RULE_L, 1ll, 1ll, 0ll, 1ll), out);
}

void jet_rule_power(JetRuleBase base, long long a_numerator, long long a_denominator, long long b_numerator,
                    long long b_denominator, JetRule *out)
{
    static const unsigned int places[3] = {JET_RULE_X, JET_RULE_D, JET_RULE_TWO};
    jet_rule_single(jet_rule_factor(places[base], a_numerator, a_denominator, b_numerator, b_denominator), out);
}

// the place named, a new one where none is; past the table's bound the first named place, refused
static unsigned int jet_rule_place(const char *name, int constant, unsigned int function, unsigned int a,
                                   unsigned int b)
{
    for (unsigned int index = 0u; index < s_jet_rule_place_count; index += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_jet_rule_places[index].name, .cap = JET_RULE_NAME,
                       .other = name, .other_cap = JET_RULE_NAME))
        {
            return JET_RULE_NAMED + index;
        }
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = JET_RULE_NAME);
    if ((s_jet_rule_place_count >= JET_RULE_PLACES) || (length >= JET_RULE_NAME))
    {
        s_jet_rule_wide = 1;
        return JET_RULE_NAMED;
    }
    JetRulePlace *const place = &s_jet_rule_places[s_jet_rule_place_count];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = place->name, .src = name, .bytes = length);
    place->name[length] = '\0';
    place->constant = constant;
    place->function = function;
    place->a = a;
    place->b = b;
    s_jet_rule_place_count += 1u;
    return JET_RULE_NAMED + s_jet_rule_place_count - 1u;
}

void jet_rule_constant(const char *name, JetRule *out)
{
    char named[JET_RULE_NAME];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_ch(&line, '#');
    mmgr_line_put(&line, name);
    s_jet_rule_wide |= mmgr_line_ok(&line) ? 0 : 1;
    (void)mmgr_line_finish(&line);
    jet_rule_single(jet_rule_factor(jet_rule_place(named, 1, 0u, 0u, 0u), 1ll, 1ll, 0ll, 1ll), out);
}

unsigned int jet_rule_function(const char *name)
{
    for (unsigned int index = 0u; index < s_jet_rule_function_count; index += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_jet_rule_functions[index].name, .cap = JET_RULE_NAME,
                       .other = name, .other_cap = JET_RULE_NAME))
        {
            return index;
        }
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = JET_RULE_NAME);
    if ((s_jet_rule_function_count >= JET_RULE_FUNCTIONS) || (length >= JET_RULE_NAME))
    {
        s_jet_rule_wide = 1;
        return 0u;
    }
    JetRuleFunction *const function = &s_jet_rule_functions[s_jet_rule_function_count];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = function->name, .src = name, .bytes = length);
    function->name[length] = '\0';
    function->related = 0;
    function->order = 0u;
    function->value = JetRule JET_RULE_NONE;
    s_jet_rule_function_count += 1u;
    return s_jet_rule_function_count - 1u;
}

// a copy of `rule` into `out`, on the temporary tier, or on the persistent tier where `ephemeral` is 0
static void jet_rule_copied(const JetRule *rule, JetRule *out, int ephemeral)
{
    JetRule made;
    jet_rule_taken(&made, rule->count, ephemeral);
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        jet_rule_term_pushed(&made, &rule->term[at].monomial, rule->term[at].value);
    }
    jet_rule_released(out);
    *out = made;
}

void jet_rule_relate(unsigned int function, unsigned int order, const JetRule *value)
{
    s_jet_rule_functions[function].related = 1;
    s_jet_rule_functions[function].order = order;
    jet_rule_copied(value, &s_jet_rule_functions[function].value, 0);
}

void jet_rule_slope(unsigned int function, unsigned int a, unsigned int b, JetRule *out)
{
    const JetRuleFunction *const stored = &s_jet_rule_functions[function];
    if (stored->related && (a >= stored->order))
    {
        JetRule value = JET_RULE_NONE;
        jet_rule_copied(&stored->value, &value, 1);
        for (unsigned int step = stored->order; step < a; step += 1u)
        {
            jet_rule_x(&value, &value);
        }
        for (unsigned int step = 0u; step < b; step += 1u)
        {
            jet_rule_slope_eta(&value, &value);
        }
        jet_rule_released(out);
        *out = value;
        return;
    }
    char name[JET_RULE_NAME];
    VerbaLinea line = {name, sizeof(name), 0ull};
    mmgr_line_put(&line, stored->name);
    mmgr_line_ch(&line, '_');
    mmgr_line_decimal(&line, a, 1u);
    mmgr_line_ch(&line, '_');
    mmgr_line_decimal(&line, b, 1u);
    s_jet_rule_wide |= mmgr_line_ok(&line) ? 0 : 1;
    (void)mmgr_line_finish(&line);
    jet_rule_single(jet_rule_factor(jet_rule_place(name, 0, function, a, b), 1ll, 1ll, 0ll, 1ll), out);
}

void jet_rule_sum(const JetRule *left, const JetRule *right, JetRule *out)
{
    JetRule made;
    jet_rule_taken(&made, left->count + (JET_RULE_SPREAD * right->count), 1);
    for (unsigned int at = 0u; at < left->count; at += 1u)
    {
        jet_rule_term_pushed(&made, &left->term[at].monomial, left->term[at].value);
    }
    JetRuleWork work;
    for (unsigned int at = 0u; at < right->count; at += 1u)
    {
        jet_rule_work_of(&right->term[at].monomial, &work);
        jet_rule_add(&made, &work, right->term[at].value);
    }
    jet_rule_settled(&made, out);
}

void jet_rule_scaled(const JetRule *rule, SimRational factor, JetRule *out)
{
    JetRule made;
    jet_rule_taken(&made, JET_RULE_SPREAD * rule->count, 1);
    JetRuleWork work;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        jet_rule_work_of(&rule->term[at].monomial, &work);
        jet_rule_add(&made, &work, sim_rational_product(rule->term[at].value, factor));
    }
    jet_rule_settled(&made, out);
}

void jet_rule_difference(const JetRule *left, const JetRule *right, JetRule *out)
{
    JetRule turned = JET_RULE_NONE;
    jet_rule_scaled(right, sim_rational(-1ll, 1ll), &turned);
    jet_rule_sum(left, &turned, out);
    jet_rule_released(&turned);
}

void jet_rule_product(const JetRule *left, const JetRule *right, JetRule *out)
{
    JetRule made;
    jet_rule_taken(&made, JET_RULE_SPREAD * left->count * right->count, 1);
    JetRuleWork work;
    for (unsigned int one = 0u; one < left->count; one += 1u)
    {
        for (unsigned int other = 0u; other < right->count; other += 1u)
        {
            jet_rule_work_of(&left->term[one].monomial, &work);
            for (unsigned int index = 0u; index < right->term[other].monomial.count; index += 1u)
            {
                jet_rule_work_pushed(&work, right->term[other].monomial.factor[index]);
            }
            jet_rule_add(&made, &work, sim_rational_product(left->term[one].value, right->term[other].value));
        }
    }
    jet_rule_settled(&made, out);
}

// the product with one factor's exponent lowered by 1, times `coefficient` times the exponent a + b h
static void jet_rule_lowered(const JetRuleMonomial *monomial, unsigned int index, SimRational coefficient, JetRule *out)
{
    const JetRuleFactor factor = monomial->factor[index];
    JetRule made;
    jet_rule_taken(&made, 2u * JET_RULE_SPREAD, 1);
    JetRuleWork rest;
    jet_rule_work_of(monomial, &rest);
    jet_rule_work_pushed(&rest, jet_rule_factor(factor.place, -1ll, 1ll, 0ll, 1ll));
    JetRuleWork lowered = rest;
    jet_rule_add(&made, &lowered,
                 sim_rational_product(coefficient, sim_rational(factor.a_numerator, factor.a_denominator)));
    if (factor.b_numerator != 0ll)
    {
        jet_rule_work_pushed(&rest, jet_rule_factor(JET_RULE_ANISOTROPY, 1ll, 1ll, 0ll, 1ll));
        jet_rule_add(&made, &rest,
                     sim_rational_product(coefficient, sim_rational(factor.b_numerator, factor.b_denominator)));
    }
    jet_rule_settled(&made, out);
}

// `left` times the product of the expressions `right` and `third`
static void jet_rule_product_of(const JetRule *left, const JetRule *right, const JetRule *third, JetRule *out)
{
    JetRule both = JET_RULE_NONE;
    jet_rule_product(right, third, &both);
    jet_rule_product(left, &both, out);
    jet_rule_released(&both);
}

// the derivative of one place's base in X, where `along_x`, or in eta
static void jet_rule_base_slope(unsigned int place, int along_x, JetRule *out)
{
    JetRule none = JET_RULE_NONE;
    if (place == JET_RULE_X)
    {
        if (along_x)
        {
            jet_rule_number(1ll, 1ll, out);
        }
        else
        {
            jet_rule_copied(&none, out, 1);
        }
        return;
    }
    if (place == JET_RULE_ETA)
    {
        if (along_x)
        {
            jet_rule_copied(&none, out, 1);
        }
        else
        {
            jet_rule_number(1ll, 1ll, out);
        }
        return;
    }
    if (place == JET_RULE_D)
    {
        jet_rule_copied(&none, out, 1);
        if (!along_x)
        {
            jet_rule_eta(out);
            jet_rule_scaled(out, sim_rational(-2ll, 1ll), out);
        }
        return;
    }
    if (place == JET_RULE_L)
    {
        jet_rule_copied(&none, out, 1);
        if (!along_x)
        {
            // d/deta L^-1 = 4 h eta L^-2, as the base of L^-1 to the power 1
            JetRule h = JET_RULE_NONE;
            JetRule eta = JET_RULE_NONE;
            JetRule over_l = JET_RULE_NONE;
            JetRule part = JET_RULE_NONE;
            jet_rule_h(&h);
            jet_rule_eta(&eta);
            jet_rule_over_l(&over_l);
            jet_rule_product(&over_l, &over_l, &part);
            jet_rule_product(&eta, &part, &part);
            jet_rule_product(&h, &part, &part);
            jet_rule_scaled(&part, sim_rational(4ll, 1ll), out);
            JetRule *const done[4] = {&h, &eta, &over_l, &part};
            for (unsigned int each = 0u; each < 4u; each += 1u)
            {
                jet_rule_released(done[each]);
            }
        }
        return;
    }
    if ((place == JET_RULE_W) || (place == JET_RULE_W + 1u))
    {
        // z_X = 1 / (2 d), z_eta = X eta d^-2
        JetRule next = JET_RULE_NONE;
        JetRule power = JET_RULE_NONE;
        jet_rule_w(place - JET_RULE_W + 1u, &next);
        if (along_x)
        {
            jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, 0ll, 1ll, &power);
            jet_rule_product(&next, &power, out);
            jet_rule_scaled(out, sim_rational(1ll, 2ll), out);
        }
        else
        {
            JetRule x = JET_RULE_NONE;
            JetRule eta = JET_RULE_NONE;
            JetRule part = JET_RULE_NONE;
            jet_rule_power(JET_RULE_BASE_X, 1ll, 1ll, 0ll, 1ll, &x);
            jet_rule_eta(&eta);
            jet_rule_power(JET_RULE_BASE_D, -2ll, 1ll, 0ll, 1ll, &power);
            jet_rule_product(&eta, &power, &part);
            jet_rule_product(&x, &part, &part);
            jet_rule_product(&next, &part, out);
            jet_rule_released(&x);
            jet_rule_released(&eta);
            jet_rule_released(&part);
        }
        jet_rule_released(&next);
        jet_rule_released(&power);
        return;
    }
    if ((place >= JET_RULE_NAMED) && (place - JET_RULE_NAMED < s_jet_rule_place_count))
    {
        const JetRulePlace named = s_jet_rule_places[place - JET_RULE_NAMED];
        if (named.constant)
        {
            jet_rule_copied(&none, out, 1);
            return;
        }
        if (along_x)
        {
            jet_rule_slope(named.function, named.a + 1u, named.b, out);
        }
        else
        {
            jet_rule_slope(named.function, named.a, named.b + 1u, out);
        }
        return;
    }
    jet_rule_copied(&none, out, 1);
}

static void jet_rule_derivative(const JetRule *rule, int along_x, JetRule *out)
{
    JetRule slope = JET_RULE_NONE;
    JetRule base = JET_RULE_NONE;
    JetRule lowered = JET_RULE_NONE;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        for (unsigned int index = 0u; index < rule->term[at].monomial.count; index += 1u)
        {
            jet_rule_base_slope(rule->term[at].monomial.factor[index].place, along_x, &base);
            if (base.count == 0u)
            {
                continue;
            }
            jet_rule_lowered(&rule->term[at].monomial, index, rule->term[at].value, &lowered);
            jet_rule_product(&lowered, &base, &lowered);
            jet_rule_sum(&slope, &lowered, &slope);
        }
    }
    jet_rule_released(&base);
    jet_rule_released(&lowered);
    jet_rule_released(out);
    *out = slope;
}

void jet_rule_x(const JetRule *rule, JetRule *out)
{
    jet_rule_derivative(rule, 1, out);
}

void jet_rule_slope_eta(const JetRule *rule, JetRule *out)
{
    jet_rule_derivative(rule, 0, out);
}

// w'' = w' - 4 d X^-1 w' + 2 (1 + h) d X^-1 w, from z w'' + (2 - z) w' - (1 + h) w = 0 and 1 / z = 2 d / X. Past it,
// d/dz = 2 d d/dX at fixed eta
void jet_rule_w(unsigned int n, JetRule *out)
{
    if (n < 2u)
    {
        jet_rule_single(jet_rule_factor(JET_RULE_W + n, 1ll, 1ll, 0ll, 1ll), out);
        return;
    }
    JetRule d = JET_RULE_NONE;
    jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll, &d);
    if (n == 2u)
    {
        JetRule over = JET_RULE_NONE;
        JetRule x = JET_RULE_NONE;
        JetRule value = JET_RULE_NONE;
        JetRule part = JET_RULE_NONE;
        JetRule other = JET_RULE_NONE;
        jet_rule_power(JET_RULE_BASE_X, -1ll, 1ll, 0ll, 1ll, &x);
        jet_rule_product(&d, &x, &over);
        jet_rule_w(1u, &value);
        jet_rule_w(1u, &part);
        jet_rule_product(&over, &part, &part);
        jet_rule_scaled(&part, sim_rational(4ll, 1ll), &part);
        jet_rule_difference(&value, &part, &value);
        jet_rule_number(1ll, 1ll, &part);
        jet_rule_h(&other);
        jet_rule_sum(&part, &other, &part);
        jet_rule_w(0u, &other);
        jet_rule_product(&over, &other, &other);
        jet_rule_product(&part, &other, &part);
        jet_rule_scaled(&part, sim_rational(2ll, 1ll), &part);
        jet_rule_sum(&value, &part, out);
        JetRule *const done[6] = {&d, &over, &x, &value, &part, &other};
        for (unsigned int each = 0u; each < 6u; each += 1u)
        {
            jet_rule_released(done[each]);
        }
        return;
    }
    JetRule before = JET_RULE_NONE;
    jet_rule_w(n - 1u, &before);
    jet_rule_x(&before, &before);
    jet_rule_product(&d, &before, &before);
    jet_rule_scaled(&before, sim_rational(2ll, 1ll), out);
    jet_rule_released(&d);
    jet_rule_released(&before);
}

int jet_rule_zero(const JetRule *rule)
{
    long long max = 0ll;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        for (unsigned int index = 0u; index < rule->term[at].monomial.count; index += 1u)
        {
            const JetRuleFactor *const factor = &rule->term[at].monomial.factor[index];
            if ((factor->place == JET_RULE_L) && (factor->a_numerator > max))
            {
                max = factor->a_numerator;
            }
        }
    }
    // L = (1 - 2 h) + 2 h d
    JetRule l = JET_RULE_NONE;
    JetRule part = JET_RULE_NONE;
    JetRule other = JET_RULE_NONE;
    jet_rule_number(1ll, 1ll, &l);
    jet_rule_h(&part);
    jet_rule_scaled(&part, sim_rational(2ll, 1ll), &part);
    jet_rule_difference(&l, &part, &l);
    jet_rule_h(&part);
    jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll, &other);
    jet_rule_product(&part, &other, &part);
    jet_rule_scaled(&part, sim_rational(2ll, 1ll), &part);
    jet_rule_sum(&l, &part, &l);
    JetRule cleared = JET_RULE_NONE;
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        JetRuleWork rest;
        rest.count = 0u;
        long long power = 0ll;
        for (unsigned int index = 0u; index < rule->term[at].monomial.count; index += 1u)
        {
            const JetRuleFactor *const factor = &rule->term[at].monomial.factor[index];
            if (factor->place == JET_RULE_L)
            {
                power = factor->a_numerator;
            }
            else
            {
                jet_rule_work_pushed(&rest, *factor);
            }
        }
        JetRule made;
        jet_rule_taken(&made, JET_RULE_SPREAD, 1);
        jet_rule_add(&made, &rest, rule->term[at].value);
        jet_rule_settled(&made, &part);
        for (long long step = power; step < max; step += 1ll)
        {
            jet_rule_product(&part, &l, &part);
        }
        jet_rule_sum(&cleared, &part, &cleared);
    }
    const int zero = cleared.count == 0u;
    JetRule *const done[4] = {&l, &part, &other, &cleared};
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        jet_rule_released(done[each]);
    }
    return zero;
}

// a + b h, as the text writes an exponent, after the bytes of `line`
static void jet_rule_exponent_text(const JetRuleFactor *factor, VerbaLinea *line)
{
    if (factor->a_numerator != 0ll)
    {
        mmgr_line_decimal_signed(line, factor->a_numerator);
        if (factor->a_denominator != 1ll)
        {
            mmgr_line_ch(line, '/');
            mmgr_line_decimal_signed(line, factor->a_denominator);
        }
    }
    if (factor->b_numerator != 0ll)
    {
        mmgr_line_put(line, (factor->a_numerator == 0ll) ? "" : " + ");
        mmgr_line_decimal_signed(line, factor->b_numerator);
        if (factor->b_denominator != 1ll)
        {
            mmgr_line_ch(line, '/');
            mmgr_line_decimal_signed(line, factor->b_denominator);
        }
        mmgr_line_put(line, " h");
    }
}

void jet_rule_text(const JetRule *rule, VerbaLinea *line)
{
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        record_rational(line, rule->term[at].value);
        for (unsigned int index = 0u; index < rule->term[at].monomial.count; index += 1u)
        {
            const JetRuleFactor *const factor = &rule->term[at].monomial.factor[index];
            mmgr_line_ch(line, ' ');
            if (factor->place == JET_RULE_ETA)
            {
                mmgr_line_put(line, "eta");
            }
            else if (factor->place == JET_RULE_ANISOTROPY)
            {
                mmgr_line_put(line, "h");
            }
            else if (factor->place == JET_RULE_L)
            {
                mmgr_line_put(line, "L^-1");
            }
            else if (factor->place == JET_RULE_X)
            {
                mmgr_line_put(line, "X");
            }
            else if (factor->place == JET_RULE_D)
            {
                mmgr_line_put(line, "d");
            }
            else if (factor->place == JET_RULE_TWO)
            {
                mmgr_line_put(line, "2");
            }
            else if (factor->place < JET_RULE_NAMED)
            {
                mmgr_line_put(line, (factor->place == JET_RULE_W) ? "w" : "w'");
            }
            else if (factor->place - JET_RULE_NAMED < s_jet_rule_place_count)
            {
                const JetRulePlace *const named = &s_jet_rule_places[factor->place - JET_RULE_NAMED];
                if (named->constant)
                {
                    mmgr_line_put(line, named->name + 1);
                }
                else
                {
                    mmgr_line_put(line, s_jet_rule_functions[named->function].name);
                    mmgr_line_put(line, "_X");
                    mmgr_line_decimal(line, named->a, 1u);
                    mmgr_line_put(line, "_eta");
                    mmgr_line_decimal(line, named->b, 1u);
                }
            }
            const int whole_one =
                (factor->a_numerator == 1ll) && (factor->a_denominator == 1ll) && (factor->b_numerator == 0ll);
            if (!whole_one)
            {
                mmgr_line_put(line, "^(");
                jet_rule_exponent_text(factor, line);
                mmgr_line_ch(line, ')');
            }
        }
        mmgr_line_ch(line, '\n');
    }
    if (rule->count == 0u)
    {
        mmgr_line_put(line, "0\n");
    }
}

unsigned int jet_rule_terms(const JetRule *rule)
{
    return rule->count;
}

int jet_rule_short(void)
{
    return (g_sim_rational_wide != 0) || (s_jet_rule_wide != 0);
}
