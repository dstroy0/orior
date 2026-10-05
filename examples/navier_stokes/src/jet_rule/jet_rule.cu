// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jet_rule.cu: held functions and their relations, in one normal form (jet_rule.h)
#include "jet_rule.h"

#include "term_form.h"

// the places of the factors: the fixed ones, then constants and the derivatives of held functions as they are named
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

// a named place: a constant, or a derivative of a held function
typedef struct
{
    std::string name;
    int constant;
    unsigned int function;
    unsigned int a;
    unsigned int b;
} JetRulePlace;

typedef struct
{
    std::string name;
    int related;
    unsigned int order;
    JetRule value;
} JetRuleFunction;

static std::vector<JetRulePlace> s_jet_rule_places;
static std::map<std::string, unsigned int> s_jet_rule_place_of;
static std::vector<JetRuleFunction> s_jet_rule_functions;
static int s_jet_rule_wide = 0;

// an exponent past this is taken as past a word
static const long long s_jet_rule_exponent_most = 1ll << 40;

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
    if ((*numerator > s_jet_rule_exponent_most) || (*numerator < -s_jet_rule_exponent_most) ||
        (*denominator > s_jet_rule_exponent_most))
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

bool JetRuleMonomialOrder::operator()(const JetRuleMonomial &left, const JetRuleMonomial &right) const
{
    const size_t size = (left.size() < right.size()) ? left.size() : right.size();
    for (size_t index = 0u; index < size; index += 1u)
    {
        const JetRuleFactor &one = left[index];
        const JetRuleFactor &other = right[index];
        const long long first[5] = {one.place, one.a_numerator, one.a_denominator, one.b_numerator, one.b_denominator};
        const long long second[5] = {other.place, other.a_numerator, other.a_denominator, other.b_numerator, other.b_denominator};
        for (unsigned int part = 0u; part < 5u; part += 1u)
        {
            if (first[part] != second[part])
            {
                return first[part] < second[part];
            }
        }
    }
    return left.size() < right.size();
}

static JetRuleFactor jet_rule_factor(unsigned int place, long long a_numerator, long long a_denominator, long long b_numerator,
                                     long long b_denominator)
{
    JetRuleFactor factor = {place, a_numerator, a_denominator, b_numerator, b_denominator};
    jet_rule_settle(&factor.a_numerator, &factor.a_denominator);
    jet_rule_settle(&factor.b_numerator, &factor.b_denominator);
    return factor;
}

// the monomial's factors at one place each, in order, exponent 0 removed; whole powers of 2 moved into `coefficient`
static JetRuleMonomial jet_rule_tidy(JetRuleMonomial monomial, SimRational *coefficient)
{
    std::map<unsigned int, JetRuleFactor> merged;
    for (const JetRuleFactor &factor : monomial)
    {
        const auto found = merged.find(factor.place);
        if (found == merged.end())
        {
            merged[factor.place] = factor;
            continue;
        }
        jet_rule_add_exponent(&found->second.a_numerator, &found->second.a_denominator, factor.a_numerator, factor.a_denominator);
        jet_rule_add_exponent(&found->second.b_numerator, &found->second.b_denominator, factor.b_numerator, factor.b_denominator);
    }
    JetRuleMonomial tidy;
    for (auto &entry : merged)
    {
        JetRuleFactor &factor = entry.second;
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
            for (long long step = 0ll; step < size; step += 1ll)
            {
                *coefficient = sim_rational_product(*coefficient, (whole > 0ll) ? sim_rational(2ll, 1ll) : sim_rational(1ll, 2ll));
            }
        }
        if ((factor.a_numerator != 0ll) || (factor.b_numerator != 0ll))
        {
            tidy.push_back(factor);
        }
    }
    return tidy;
}

// the exponent of eta in the monomial
static long long jet_rule_eta_power(const JetRuleMonomial &monomial)
{
    for (const JetRuleFactor &factor : monomial)
    {
        if (factor.place == JET_RULE_ETA)
        {
            return factor.a_numerator;
        }
    }
    return 0ll;
}

// the term added, eta^2 written as 1 - d
static void jet_rule_add(JetRule *rule, JetRuleMonomial monomial, SimRational coefficient)
{
    if (sim_rational_sign(coefficient) == 0)
    {
        return;
    }
    monomial = jet_rule_tidy(monomial, &coefficient);
    if (jet_rule_eta_power(monomial) >= 2ll)
    {
        JetRuleMonomial lower = monomial;
        lower.push_back(jet_rule_factor(JET_RULE_ETA, -2ll, 1ll, 0ll, 1ll));
        jet_rule_add(rule, lower, coefficient);
        lower.push_back(jet_rule_factor(JET_RULE_D, 1ll, 1ll, 0ll, 1ll));
        jet_rule_add(rule, lower, sim_rational_negative(coefficient));
        return;
    }
    const auto found = rule->find(monomial);
    if (found == rule->end())
    {
        (*rule)[monomial] = coefficient;
        return;
    }
    found->second = sim_rational_sum(found->second, coefficient);
    if (sim_rational_sign(found->second) == 0)
    {
        rule->erase(found);
    }
}

static JetRule jet_rule_single(const JetRuleFactor &factor)
{
    JetRule rule;
    jet_rule_add(&rule, JetRuleMonomial(1u, factor), sim_rational(1ll, 1ll));
    return rule;
}

JetRule jet_rule_rational(SimRational value)
{
    JetRule rule;
    jet_rule_add(&rule, JetRuleMonomial(), value);
    return rule;
}

JetRule jet_rule_number(long long numerator, long long denominator)
{
    return jet_rule_rational(sim_rational(numerator, denominator));
}

JetRule jet_rule_eta(void)
{
    return jet_rule_single(jet_rule_factor(JET_RULE_ETA, 1ll, 1ll, 0ll, 1ll));
}

JetRule jet_rule_h(void)
{
    return jet_rule_single(jet_rule_factor(JET_RULE_ANISOTROPY, 1ll, 1ll, 0ll, 1ll));
}

JetRule jet_rule_over_l(void)
{
    return jet_rule_single(jet_rule_factor(JET_RULE_L, 1ll, 1ll, 0ll, 1ll));
}

JetRule jet_rule_power(JetRuleBase base, long long a_numerator, long long a_denominator, long long b_numerator,
                       long long b_denominator)
{
    static const unsigned int places[3] = {JET_RULE_X, JET_RULE_D, JET_RULE_TWO};
    return jet_rule_single(jet_rule_factor(places[base], a_numerator, a_denominator, b_numerator, b_denominator));
}

// the place named, a new one where none is
static unsigned int jet_rule_place(const std::string &name, int constant, unsigned int function, unsigned int a, unsigned int b)
{
    const auto found = s_jet_rule_place_of.find(name);
    if (found != s_jet_rule_place_of.end())
    {
        return found->second;
    }
    const unsigned int place = JET_RULE_NAMED + (unsigned int)s_jet_rule_places.size();
    s_jet_rule_places.push_back(JetRulePlace{name, constant, function, a, b});
    s_jet_rule_place_of[name] = place;
    return place;
}

JetRule jet_rule_constant(const char *name)
{
    return jet_rule_single(jet_rule_factor(jet_rule_place(std::string("#") + name, 1, 0u, 0u, 0u), 1ll, 1ll, 0ll, 1ll));
}

unsigned int jet_rule_function(const char *name)
{
    for (size_t index = 0u; index < s_jet_rule_functions.size(); index += 1u)
    {
        if (s_jet_rule_functions[index].name == name)
        {
            return (unsigned int)index;
        }
    }
    s_jet_rule_functions.push_back(JetRuleFunction{name, 0, 0u, JetRule()});
    return (unsigned int)(s_jet_rule_functions.size() - 1u);
}

void jet_rule_relate(unsigned int function, unsigned int order, const JetRule &value)
{
    s_jet_rule_functions[function].related = 1;
    s_jet_rule_functions[function].order = order;
    s_jet_rule_functions[function].value = value;
}

JetRule jet_rule_slope(unsigned int function, unsigned int a, unsigned int b)
{
    const JetRuleFunction &held = s_jet_rule_functions[function];
    if (held.related && (a >= held.order))
    {
        JetRule value = held.value;
        for (unsigned int step = held.order; step < a; step += 1u)
        {
            value = jet_rule_x(value);
        }
        for (unsigned int step = 0u; step < b; step += 1u)
        {
            value = jet_rule_slope_eta(value);
        }
        return value;
    }
    const std::string name = held.name + "_" + std::to_string(a) + "_" + std::to_string(b);
    return jet_rule_single(jet_rule_factor(jet_rule_place(name, 0, function, a, b), 1ll, 1ll, 0ll, 1ll));
}

// w'' = w' - 4 d X^-1 w' + 2 (1 + h) d X^-1 w, from z w'' + (2 - z) w' - (1 + h) w = 0 and 1 / z = 2 d / X; past it,
// d/dz = 2 d d/dX at fixed eta
JetRule jet_rule_w(unsigned int n)
{
    if (n < 2u)
    {
        return jet_rule_single(jet_rule_factor(JET_RULE_W + n, 1ll, 1ll, 0ll, 1ll));
    }
    if (n == 2u)
    {
        const JetRule over = jet_rule_product(jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll),
                                              jet_rule_power(JET_RULE_BASE_X, -1ll, 1ll, 0ll, 1ll));
        JetRule value = jet_rule_w(1u);
        value = jet_rule_difference(value, jet_rule_scaled(jet_rule_product(over, jet_rule_w(1u)), sim_rational(4ll, 1ll)));
        const JetRule one_h = jet_rule_sum(jet_rule_number(1ll, 1ll), jet_rule_h());
        value = jet_rule_sum(value, jet_rule_scaled(jet_rule_product(one_h, jet_rule_product(over, jet_rule_w(0u))), sim_rational(2ll, 1ll)));
        return value;
    }
    return jet_rule_scaled(jet_rule_product(jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll), jet_rule_x(jet_rule_w(n - 1u))),
                           sim_rational(2ll, 1ll));
}

JetRule jet_rule_sum(const JetRule &left, const JetRule &right)
{
    JetRule sum = left;
    for (const auto &term : right)
    {
        jet_rule_add(&sum, term.first, term.second);
    }
    return sum;
}

JetRule jet_rule_scaled(const JetRule &rule, SimRational factor)
{
    JetRule scaled;
    for (const auto &term : rule)
    {
        jet_rule_add(&scaled, term.first, sim_rational_product(term.second, factor));
    }
    return scaled;
}

JetRule jet_rule_difference(const JetRule &left, const JetRule &right)
{
    return jet_rule_sum(left, jet_rule_scaled(right, sim_rational(-1ll, 1ll)));
}

JetRule jet_rule_product(const JetRule &left, const JetRule &right)
{
    JetRule product;
    for (const auto &first : left)
    {
        for (const auto &second : right)
        {
            JetRuleMonomial monomial = first.first;
            monomial.insert(monomial.end(), second.first.begin(), second.first.end());
            jet_rule_add(&product, monomial, sim_rational_product(first.second, second.second));
        }
    }
    return product;
}

// the monomial with one factor's exponent lowered by 1, times `coefficient` times the exponent a + b h
static JetRule jet_rule_lowered(const JetRuleMonomial &monomial, size_t index, SimRational coefficient)
{
    const JetRuleFactor &factor = monomial[index];
    JetRuleMonomial rest = monomial;
    rest.push_back(jet_rule_factor(factor.place, -1ll, 1ll, 0ll, 1ll));
    JetRule lowered;
    jet_rule_add(&lowered, rest, sim_rational_product(coefficient, sim_rational(factor.a_numerator, factor.a_denominator)));
    if (factor.b_numerator != 0ll)
    {
        rest.push_back(jet_rule_factor(JET_RULE_ANISOTROPY, 1ll, 1ll, 0ll, 1ll));
        jet_rule_add(&lowered, rest, sim_rational_product(coefficient, sim_rational(factor.b_numerator, factor.b_denominator)));
    }
    return lowered;
}

// the derivative of one place's base in X, where `along_x`, or in eta
static JetRule jet_rule_base_slope(unsigned int place, int along_x)
{
    if (place == JET_RULE_X)
    {
        return along_x ? jet_rule_number(1ll, 1ll) : JetRule();
    }
    if (place == JET_RULE_ETA)
    {
        return along_x ? JetRule() : jet_rule_number(1ll, 1ll);
    }
    if (place == JET_RULE_D)
    {
        return along_x ? JetRule() : jet_rule_scaled(jet_rule_eta(), sim_rational(-2ll, 1ll));
    }
    if (place == JET_RULE_L)
    {
        // d/deta L^-1 = 4 h eta L^-2, as the base of L^-1 to the power 1
        return along_x ? JetRule()
                       : jet_rule_scaled(jet_rule_product(jet_rule_h(), jet_rule_product(jet_rule_eta(), jet_rule_product(jet_rule_over_l(), jet_rule_over_l()))),
                                         sim_rational(4ll, 1ll));
    }
    if ((place == JET_RULE_W) || (place == JET_RULE_W + 1u))
    {
        // z_X = 1 / (2 d), z_eta = X eta d^-2
        const JetRule next = jet_rule_w(place - JET_RULE_W + 1u);
        return along_x ? jet_rule_scaled(jet_rule_product(next, jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, 0ll, 1ll)), sim_rational(1ll, 2ll))
                       : jet_rule_product(next, jet_rule_product(jet_rule_power(JET_RULE_BASE_X, 1ll, 1ll, 0ll, 1ll),
                                                                 jet_rule_product(jet_rule_eta(), jet_rule_power(JET_RULE_BASE_D, -2ll, 1ll, 0ll, 1ll))));
    }
    if (place >= JET_RULE_NAMED)
    {
        const JetRulePlace &named = s_jet_rule_places[place - JET_RULE_NAMED];
        if (named.constant)
        {
            return JetRule();
        }
        return along_x ? jet_rule_slope(named.function, named.a + 1u, named.b) : jet_rule_slope(named.function, named.a, named.b + 1u);
    }
    return JetRule();
}

static JetRule jet_rule_derivative(const JetRule &rule, int along_x)
{
    JetRule slope;
    for (const auto &term : rule)
    {
        for (size_t index = 0u; index < term.first.size(); index += 1u)
        {
            const JetRule base = jet_rule_base_slope(term.first[index].place, along_x);
            if (base.empty())
            {
                continue;
            }
            slope = jet_rule_sum(slope, jet_rule_product(jet_rule_lowered(term.first, index, term.second), base));
        }
    }
    return slope;
}

JetRule jet_rule_x(const JetRule &rule)
{
    return jet_rule_derivative(rule, 1);
}

JetRule jet_rule_slope_eta(const JetRule &rule)
{
    return jet_rule_derivative(rule, 0);
}

int jet_rule_zero(const JetRule &rule)
{
    long long most = 0ll;
    for (const auto &term : rule)
    {
        for (const JetRuleFactor &factor : term.first)
        {
            if ((factor.place == JET_RULE_L) && (factor.a_numerator > most))
            {
                most = factor.a_numerator;
            }
        }
    }
    // L = (1 - 2 h) + 2 h d
    JetRule l = jet_rule_difference(jet_rule_number(1ll, 1ll), jet_rule_scaled(jet_rule_h(), sim_rational(2ll, 1ll)));
    l = jet_rule_sum(l, jet_rule_scaled(jet_rule_product(jet_rule_h(), jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll)), sim_rational(2ll, 1ll)));
    JetRule cleared;
    for (const auto &term : rule)
    {
        JetRuleMonomial rest;
        long long power = 0ll;
        for (const JetRuleFactor &factor : term.first)
        {
            if (factor.place == JET_RULE_L)
            {
                power = factor.a_numerator;
            }
            else
            {
                rest.push_back(factor);
            }
        }
        JetRule part;
        jet_rule_add(&part, rest, term.second);
        for (long long step = power; step < most; step += 1ll)
        {
            part = jet_rule_product(part, l);
        }
        cleared = jet_rule_sum(cleared, part);
    }
    return cleared.empty();
}

static std::string jet_rule_exponent_text(const JetRuleFactor &factor)
{
    std::string text;
    if (factor.a_numerator != 0ll)
    {
        text = std::to_string(factor.a_numerator) + ((factor.a_denominator != 1ll) ? "/" + std::to_string(factor.a_denominator) : "");
    }
    if (factor.b_numerator != 0ll)
    {
        text += (text.empty() ? "" : " + ") + std::to_string(factor.b_numerator) +
                ((factor.b_denominator != 1ll) ? "/" + std::to_string(factor.b_denominator) : "") + " h";
    }
    return text;
}

std::string jet_rule_text(const JetRule &rule)
{
    std::string text;
    for (const auto &term : rule)
    {
        text += term_book_rational(term.second);
        for (const JetRuleFactor &factor : term.first)
        {
            std::string name;
            if (factor.place == JET_RULE_ETA)
            {
                name = "eta";
            }
            else if (factor.place == JET_RULE_ANISOTROPY)
            {
                name = "h";
            }
            else if (factor.place == JET_RULE_L)
            {
                name = "L^-1";
            }
            else if (factor.place == JET_RULE_X)
            {
                name = "X";
            }
            else if (factor.place == JET_RULE_D)
            {
                name = "d";
            }
            else if (factor.place == JET_RULE_TWO)
            {
                name = "2";
            }
            else if (factor.place < JET_RULE_NAMED)
            {
                name = (factor.place == JET_RULE_W) ? "w" : "w'";
            }
            else
            {
                const JetRulePlace &named = s_jet_rule_places[factor.place - JET_RULE_NAMED];
                name = named.constant ? named.name.substr(1u)
                                      : s_jet_rule_functions[named.function].name + "_X" + std::to_string(named.a) + "_eta" +
                                            std::to_string(named.b);
            }
            const std::string exponent = jet_rule_exponent_text(factor);
            text += " " + name + ((exponent == "1") ? std::string() : "^(" + exponent + ")");
        }
        text += "\n";
    }
    return text.empty() ? std::string("0\n") : text;
}

size_t jet_rule_terms(const JetRule &rule)
{
    return rule.size();
}

int jet_rule_short(void)
{
    return (s_sim_rational_wide != 0) || (s_jet_rule_wide != 0);
}
