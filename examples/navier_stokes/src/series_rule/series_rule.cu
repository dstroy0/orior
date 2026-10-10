// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// series_rule.cu: series held by the rule of their entries, in one normal form (series_rule.h)
#include "series_rule.h"

#include "term_form.h"

bool SeriesRuleKeyOrder::operator()(const SeriesRuleKey &left, const SeriesRuleKey &right) const
{
    const long long first[7] = {left.product,           left.first.field,  left.first.shift, left.first.slope,
                                left.second.field,      left.second.shift, left.second.slope};
    const long long second[7] = {right.product,          right.first.field,  right.first.shift, right.first.slope,
                                 right.second.field,     right.second.shift, right.second.slope};
    for (unsigned int index = 0u; index < 7u; index += 1u)
    {
        if (first[index] != second[index])
        {
            return first[index] < second[index];
        }
    }
    return false;
}

SeriesRulePolynomial series_rule_constant(SimRational value)
{
    SeriesRulePolynomial polynomial;
    if (sim_rational_sign(value) != 0)
    {
        polynomial[SeriesRulePower{0u, 0u, 0u, 0u}] = value;
    }
    return polynomial;
}

SeriesRulePolynomial series_rule_variable(SeriesRuleVariable variable)
{
    SeriesRulePower power = {0u, 0u, 0u, 0u};
    power[variable] = 1u;
    SeriesRulePolynomial polynomial;
    polynomial[power] = sim_rational(1ll, 1ll);
    return polynomial;
}

// `value` added at `power`, the term removed where the sum is 0
static void series_rule_add_term(SeriesRulePolynomial *polynomial, const SeriesRulePower &power, SimRational value)
{
    const auto found = polynomial->find(power);
    if (found == polynomial->end())
    {
        if (sim_rational_sign(value) != 0)
        {
            (*polynomial)[power] = value;
        }
        return;
    }
    found->second = sim_rational_sum(found->second, value);
    if (sim_rational_sign(found->second) == 0)
    {
        polynomial->erase(found);
    }
}

SeriesRulePolynomial series_rule_polynomial_sum(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right)
{
    SeriesRulePolynomial sum = left;
    for (const auto &term : right)
    {
        series_rule_add_term(&sum, term.first, term.second);
    }
    return sum;
}

static SeriesRulePolynomial series_rule_polynomial_scaled(const SeriesRulePolynomial &polynomial, SimRational factor)
{
    SeriesRulePolynomial scaled;
    for (const auto &term : polynomial)
    {
        series_rule_add_term(&scaled, term.first, sim_rational_product(term.second, factor));
    }
    return scaled;
}

SeriesRulePolynomial series_rule_polynomial_difference(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right)
{
    return series_rule_polynomial_sum(left, series_rule_polynomial_scaled(right, sim_rational(-1ll, 1ll)));
}

SeriesRulePolynomial series_rule_polynomial_product(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right)
{
    SeriesRulePolynomial product;
    for (const auto &first : left)
    {
        for (const auto &second : right)
        {
            SeriesRulePower power;
            for (unsigned int variable = 0u; variable < SERIES_RULE_VARIABLES; variable += 1u)
            {
                power[variable] = first.first[variable] + second.first[variable];
            }
            series_rule_add_term(&product, power, sim_rational_product(first.second, second.second));
        }
    }
    return product;
}

// the polynomial with `variable` replaced by `value`
static SeriesRulePolynomial series_rule_substitute(const SeriesRulePolynomial &polynomial, SeriesRuleVariable variable,
                                                  const SeriesRulePolynomial &value)
{
    SeriesRulePolynomial result;
    for (const auto &term : polynomial)
    {
        SeriesRulePower rest = term.first;
        rest[variable] = 0u;
        SeriesRulePolynomial part;
        part[rest] = term.second;
        for (unsigned int power = 0u; power < term.first[variable]; power += 1u)
        {
            part = series_rule_polynomial_product(part, value);
        }
        result = series_rule_polynomial_sum(result, part);
    }
    return result;
}

// k + shift, or i + shift, as a polynomial
static SeriesRulePolynomial series_rule_moved(SeriesRuleVariable variable, long long shift)
{
    return series_rule_polynomial_sum(series_rule_variable(variable), series_rule_constant(sim_rational(shift, 1ll)));
}

// L = 1 - 2 h eta^2
static SeriesRulePolynomial series_rule_l(void)
{
    SeriesRulePolynomial l = series_rule_constant(sim_rational(1ll, 1ll));
    l[SeriesRulePower{2u, 1u, 0u, 0u}] = sim_rational(-2ll, 1ll);
    return l;
}

// the polynomial times L^count
static SeriesRulePolynomial series_rule_raised(SeriesRulePolynomial polynomial, unsigned int count)
{
    const SeriesRulePolynomial l = series_rule_l();
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        polynomial = series_rule_polynomial_product(polynomial, l);
    }
    return polynomial;
}

// the part added, a Cauchy product's pair held in its order and a pair of one entry held even under i -> k + c - i
static void series_rule_add(SeriesRule *rule, SeriesRuleKey key, SeriesRuleFactor factor)
{
    if (key.product)
    {
        // i -> k + c - i
        const SeriesRulePolynomial turned = series_rule_polynomial_difference(
            series_rule_moved(SERIES_RULE_K, key.second.shift), series_rule_variable(SERIES_RULE_I));
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
            factor.numerator = series_rule_substitute(factor.numerator, SERIES_RULE_I, turned);
        }
        else if ((first[0] == second[0]) && (first[1] == second[1]))
        {
            factor.numerator = series_rule_polynomial_scaled(
                series_rule_polynomial_sum(factor.numerator, series_rule_substitute(factor.numerator, SERIES_RULE_I, turned)),
                sim_rational(1ll, 2ll));
        }
    }
    if (factor.numerator.empty())
    {
        return;
    }
    const auto found = rule->find(key);
    if (found == rule->end())
    {
        (*rule)[key] = factor;
        return;
    }
    const unsigned int power = (factor.power > found->second.power) ? factor.power : found->second.power;
    found->second.numerator = series_rule_polynomial_sum(series_rule_raised(found->second.numerator, power - found->second.power),
                                                         series_rule_raised(factor.numerator, power - factor.power));
    found->second.power = power;
    if (found->second.numerator.empty())
    {
        rule->erase(found);
    }
}

SeriesRule series_rule_part(const SeriesRulePolynomial &factor, unsigned int power, unsigned int field, int shift,
                            unsigned int slope)
{
    SeriesRule rule;
    const SeriesRuleKey key = {0, {field, shift, slope}, {0u, 0, 0u}};
    series_rule_add(&rule, key, SeriesRuleFactor{factor, power});
    return rule;
}

SeriesRule series_rule_entry(unsigned int field)
{
    return series_rule_part(series_rule_constant(sim_rational(1ll, 1ll)), 0u, field, 0, 0u);
}

SeriesRule series_rule_cauchy(const SeriesRulePolynomial &factor, unsigned int power, unsigned int first,
                              unsigned int first_slope, unsigned int second, unsigned int second_slope, int shift)
{
    SeriesRule rule;
    const SeriesRuleKey key = {1, {first, 0, first_slope}, {second, shift, second_slope}};
    series_rule_add(&rule, key, SeriesRuleFactor{factor, power});
    return rule;
}

SeriesRule series_rule_sum(const SeriesRule &left, const SeriesRule &right)
{
    SeriesRule sum = left;
    for (const auto &part : right)
    {
        series_rule_add(&sum, part.first, part.second);
    }
    return sum;
}

SeriesRule series_rule_difference(const SeriesRule &left, const SeriesRule &right)
{
    return series_rule_sum(left, series_rule_scaled(right, series_rule_constant(sim_rational(-1ll, 1ll)), 0u));
}

SeriesRule series_rule_scaled(const SeriesRule &rule, const SeriesRulePolynomial &factor, unsigned int power)
{
    SeriesRule scaled;
    for (const auto &part : rule)
    {
        series_rule_add(&scaled, part.first,
                        SeriesRuleFactor{series_rule_polynomial_product(part.second.numerator, factor), part.second.power + power});
    }
    return scaled;
}

SeriesRule series_rule_shift(const SeriesRule &rule, int shift)
{
    SeriesRule moved;
    const SeriesRulePolynomial value = series_rule_moved(SERIES_RULE_K, shift);
    for (const auto &part : rule)
    {
        SeriesRuleKey key = part.first;
        if (key.product)
        {
            key.second.shift += shift;
        }
        else
        {
            key.first.shift += shift;
        }
        series_rule_add(&moved, key,
                        SeriesRuleFactor{series_rule_substitute(part.second.numerator, SERIES_RULE_K, value), part.second.power});
    }
    return moved;
}

// (X f)_k = f_(k-1)
SeriesRule series_rule_times_x(const SeriesRule &rule)
{
    return series_rule_shift(rule, -1);
}

// (f_X)_k = (k + 1) f_(k+1)
SeriesRule series_rule_slope_x(const SeriesRule &rule)
{
    return series_rule_scaled(series_rule_shift(rule, 1), series_rule_moved(SERIES_RULE_K, 1ll), 0u);
}

// (X f_X)_k = k f_k
SeriesRule series_rule_euler(const SeriesRule &rule)
{
    return series_rule_scaled(rule, series_rule_variable(SERIES_RULE_K), 0u);
}

// (f g)_k = sum_i f_i g_(k-i); with f_i = a(i) A_(i+s) and g_(k-i) = b(k-i) B_(k-i+t), j = i + s gives
// sum_j a(j - s) b(k - j + s) A_j B_(k+s+t-j)
SeriesRule series_rule_product(const SeriesRule &left, const SeriesRule &right)
{
    SeriesRule product;
    for (const auto &first : left)
    {
        for (const auto &second : right)
        {
            const SeriesRuleEntry &one = first.first.first;
            const SeriesRuleEntry &other = second.first.first;
            const SeriesRulePolynomial at_first = series_rule_substitute(
                first.second.numerator, SERIES_RULE_K, series_rule_moved(SERIES_RULE_I, -(long long)one.shift));
            const SeriesRulePolynomial at_second = series_rule_substitute(
                second.second.numerator, SERIES_RULE_K,
                series_rule_polynomial_difference(series_rule_moved(SERIES_RULE_K, one.shift), series_rule_variable(SERIES_RULE_I)));
            const SeriesRuleKey key = {1, {one.field, 0, one.slope}, {other.field, one.shift + other.shift, other.slope}};
            series_rule_add(&product, key,
                            SeriesRuleFactor{series_rule_polynomial_product(at_first, at_second),
                                             first.second.power + second.second.power});
        }
    }
    return product;
}

// d/deta of the polynomial
static SeriesRulePolynomial series_rule_polynomial_slope(const SeriesRulePolynomial &polynomial)
{
    SeriesRulePolynomial slope;
    for (const auto &term : polynomial)
    {
        if (term.first[SERIES_RULE_ETA] == 0u)
        {
            continue;
        }
        SeriesRulePower power = term.first;
        power[SERIES_RULE_ETA] -= 1u;
        series_rule_add_term(&slope, power,
                             sim_rational_product(term.second, sim_rational((long long)term.first[SERIES_RULE_ETA], 1ll)));
    }
    return slope;
}

// d/deta (N L^-n) = (N' L + 4 n h eta N) L^-(n+1)
SeriesRule series_rule_slope_eta(const SeriesRule &rule)
{
    SeriesRule slope;
    SeriesRulePolynomial h_eta;
    h_eta[SeriesRulePower{1u, 1u, 0u, 0u}] = sim_rational(4ll, 1ll);
    for (const auto &part : rule)
    {
        const SeriesRuleFactor &factor = part.second;
        SeriesRuleFactor turned;
        if (factor.power == 0u)
        {
            turned = SeriesRuleFactor{series_rule_polynomial_slope(factor.numerator), 0u};
        }
        else
        {
            turned.numerator = series_rule_polynomial_sum(
                series_rule_polynomial_product(series_rule_polynomial_slope(factor.numerator), series_rule_l()),
                series_rule_polynomial_product(series_rule_polynomial_scaled(h_eta, sim_rational((long long)factor.power, 1ll)),
                                               factor.numerator));
            turned.power = factor.power + 1u;
        }
        series_rule_add(&slope, part.first, turned);
        SeriesRuleKey key = part.first;
        key.first.slope += 1u;
        series_rule_add(&slope, key, factor);
        if (part.first.product)
        {
            key = part.first;
            key.second.slope += 1u;
            series_rule_add(&slope, key, factor);
        }
    }
    return slope;
}

int series_rule_zero(const SeriesRule &rule)
{
    return rule.empty();
}

// the parity of one entry: its field's, turned by every derivative in eta
static int series_rule_entry_parity(const SeriesRuleEntry &entry, const std::vector<int> &parities)
{
    return ((entry.slope % 2u) == 0u) ? parities[entry.field] : -parities[entry.field];
}

int series_rule_parity(const SeriesRule &rule, const std::vector<int> &parities)
{
    int held = 0;
    for (const auto &part : rule)
    {
        int factor = 0;
        for (const auto &term : part.second.numerator)
        {
            const int parity = ((term.first[SERIES_RULE_ETA] % 2u) == 0u) ? 1 : -1;
            if ((factor != 0) && (factor != parity))
            {
                return 0;
            }
            factor = parity;
        }
        int parity = factor * series_rule_entry_parity(part.first.first, parities);
        if (part.first.product)
        {
            parity *= series_rule_entry_parity(part.first.second, parities);
        }
        if ((held != 0) && (held != parity))
        {
            return 0;
        }
        held = parity;
    }
    return held;
}

static std::string series_rule_polynomial_text(const SeriesRulePolynomial &polynomial)
{
    static const char *const names[SERIES_RULE_VARIABLES] = {"eta", "h", "k", "i"};
    std::string text;
    for (const auto &term : polynomial)
    {
        text += text.empty() ? "" : " + ";
        text += term_book_rational(term.second);
        for (unsigned int variable = 0u; variable < SERIES_RULE_VARIABLES; variable += 1u)
        {
            if (term.first[variable] == 0u)
            {
                continue;
            }
            text += std::string(" ") + names[variable];
            if (term.first[variable] > 1u)
            {
                text += "^" + std::to_string(term.first[variable]);
            }
        }
    }
    return "(" + text + ")";
}

static std::string series_rule_entry_text(const SeriesRuleEntry &entry, const char *at, const std::vector<std::string> &names)
{
    std::string text = names[entry.field];
    for (unsigned int slope = 0u; slope < entry.slope; slope += 1u)
    {
        text += "'";
    }
    text += std::string("[") + at;
    if (entry.shift != 0)
    {
        text += ((entry.shift > 0) ? " + " : " - ") + std::to_string((entry.shift > 0) ? entry.shift : -entry.shift);
    }
    return text + "]";
}

std::string series_rule_text(const SeriesRule &rule, const std::vector<std::string> &names)
{
    std::string text;
    for (const auto &part : rule)
    {
        text += series_rule_polynomial_text(part.second.numerator);
        if (part.second.power > 0u)
        {
            text += " L^-" + std::to_string(part.second.power);
        }
        if (part.first.product)
        {
            SeriesRuleEntry first = part.first.first;
            text += " sum_i " + series_rule_entry_text(first, "i", names) + " " +
                    series_rule_entry_text(part.first.second, "k - i", names);
        }
        else
        {
            text += " " + series_rule_entry_text(part.first.first, "k", names);
        }
        text += "\n";
    }
    return text.empty() ? std::string("0\n") : text;
}

size_t series_rule_parts(const SeriesRule &rule)
{
    return rule.size();
}

int series_rule_short(void)
{
    return g_sim_rational_wide != 0;
}
