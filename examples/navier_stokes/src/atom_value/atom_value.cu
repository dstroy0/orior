// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atom_value.cu: the atoms' values (atom_value.h)
#include "atom_value.h"

#include "ode_series.h"

#include <stdlib.h>

static SimRational atom_value_word(long long numerator, long long denominator)
{
    return sim_rational(numerator, denominator);
}

static SimRational atom_value_add(SimRational left, SimRational right)
{
    return sim_rational_sum(left, right);
}

static SimRational atom_value_less(SimRational left, SimRational right)
{
    return sim_rational_difference(left, right);
}

static SimRational atom_value_times(SimRational left, SimRational right)
{
    return sim_rational_product(left, right);
}

static SimRational atom_value_over(SimRational left, SimRational right)
{
    return sim_rational_product(left, sim_rational_reciprocal(right));
}

// base^power for a whole power, the reciprocal's where the power is negative
static SimRational atom_value_raised(SimRational base, long long power)
{
    SimRational value = atom_value_word(1ll, 1ll);
    const long long steps = (power < 0ll) ? -power : power;
    for (long long step = 0ll; step < steps; step += 1ll)
    {
        value = atom_value_times(value, base);
    }
    return (power < 0ll) ? sim_rational_reciprocal(value) : value;
}

// sum over k < length of x^k / k!
static SimRational atom_value_exponential(SimRational x, unsigned int length)
{
    SimRational term = atom_value_word(1ll, 1ll);
    SimRational sum = atom_value_word(0ll, 1ll);
    for (unsigned int k = 0u; k < length; k += 1u)
    {
        sum = atom_value_add(sum, term);
        term = atom_value_times(term, atom_value_over(x, atom_value_word((long long)k + 1ll, 1ll)));
    }
    return sum;
}

// sum over k < length of (a)_k / k! y^k, the series of (1 - y)^(-a)
static SimRational atom_value_binomial(SimRational y, SimRational a, unsigned int length)
{
    SimRational term = atom_value_word(1ll, 1ll);
    SimRational sum = atom_value_word(0ll, 1ll);
    for (unsigned int k = 0u; k < length; k += 1u)
    {
        sum = atom_value_add(sum, term);
        term = atom_value_times(term, atom_value_over(atom_value_times(atom_value_add(a, atom_value_word((long long)k, 1ll)), y),
                                                      atom_value_word((long long)k + 1ll, 1ll)));
    }
    return sum;
}

// the largest whole number at or below q: 1, or 0 where q passes a word
static int atom_value_whole(SimRational q, long long *whole)
{
    long long numerator = 0ll;
    long long denominator = 0ll;
    if (!sim_rational_small(&q.numerator, &numerator) || !sim_rational_small(&q.denominator, &denominator) ||
        (denominator <= 0ll))
    {
        return 0;
    }
    long long quotient = numerator / denominator;
    if (((numerator % denominator) != 0ll) && (numerator < 0ll))
    {
        quotient -= 1ll;
    }
    *whole = quotient;
    return 1;
}

SimRational atom_value_e(AtomValues *values, SimRational q)
{
    for (const AtomValuePair &pair : values->exponentials)
    {
        if (sim_rational_equal(pair.key, q))
        {
            return pair.value;
        }
    }
    long long whole = 0ll;
    if (!atom_value_whole(q, &whole))
    {
        s_sim_rational_wide = 1;
        return atom_value_word(0ll, 1ll);
    }
    const SimRational part = atom_value_less(q, atom_value_word(whole, 1ll));
    AtomValuePair pair;
    pair.key = q;
    pair.value = atom_value_raised(values->e, whole);
    if (sim_rational_sign(part) != 0)
    {
        pair.value = atom_value_times(pair.value, atom_value_exponential(part, values->length));
    }
    values->exponentials.push_back(pair);
    return pair.value;
}

SimRational atom_value_power(AtomValues *values, SimRational x, SimRational p)
{
    // |1 - 1/x| < 1 exactly where 2x - 1 > 0
    if (sim_rational_sign(atom_value_less(atom_value_times(atom_value_word(2ll, 1ll), x), atom_value_word(1ll, 1ll))) <= 0)
    {
        s_sim_rational_wide = 1;
        return atom_value_word(0ll, 1ll);
    }
    const SimRational y = atom_value_less(atom_value_word(1ll, 1ll), sim_rational_reciprocal(x));
    return atom_value_binomial(y, p, values->length);
}

// eps_1(x) to `length` levels: 1 / (b_0 + a_1 / (b_1 + ...)), b_j = x + 1 + 2j, a_j = -j^2
static SimRational atom_value_first_ratio(SimRational x, unsigned int length)
{
    SimRational tail = atom_value_add(x, atom_value_word(2ll * (long long)length - 1ll, 1ll));
    for (unsigned int level = length - 1u; level > 0u; level -= 1u)
    {
        const SimRational below = atom_value_add(x, atom_value_word(2ll * (long long)level - 1ll, 1ll));
        tail = atom_value_add(below, atom_value_over(atom_value_word(-(long long)level * (long long)level, 1ll), tail));
    }
    return sim_rational_reciprocal(tail);
}

SimRational atom_value_ratio(AtomValues *values, SimRational x, int n)
{
    size_t at = values->ratios.size();
    for (size_t index = 0u; index < values->ratios.size(); index += 1u)
    {
        if (sim_rational_equal(values->ratios[index].x, x))
        {
            at = index;
            break;
        }
    }
    if (at == values->ratios.size())
    {
        AtomValueRatios ratios;
        ratios.x = x;
        const SimRational inverse = sim_rational_reciprocal(x);
        ratios.eps.push_back(atom_value_add(inverse, atom_value_times(inverse, inverse)));
        ratios.eps.push_back(inverse);
        ratios.eps.push_back(atom_value_first_ratio(x, values->length));
        values->ratios.push_back(ratios);
    }
    std::vector<SimRational> &eps = values->ratios[at].eps;
    // entry i is eps_(i-1); eps_m = (1 - x eps_(m-1)) / (m - 1)
    while ((long long)eps.size() < (long long)n + 2ll)
    {
        const long long order = (long long)eps.size() - 1ll;
        eps.push_back(atom_value_over(atom_value_less(atom_value_word(1ll, 1ll), atom_value_times(x, eps[(size_t)order])),
                                      atom_value_word(order - 1ll, 1ll)));
    }
    return eps[(size_t)(n + 1)];
}

SimRational atom_value_gamma(AtomValues *values, SimRational s)
{
    const unsigned int length = values->length;
    // int_0^1 e^(-t) t^(s-1) dt
    SimRational lower = atom_value_word(0ll, 1ll);
    SimRational sign_factorial = atom_value_word(1ll, 1ll);
    for (unsigned int m = 0u; m < length; m += 1u)
    {
        lower = atom_value_add(lower, atom_value_over(sign_factorial, atom_value_add(s, atom_value_word((long long)m, 1ll))));
        sign_factorial = atom_value_over(sign_factorial, atom_value_word(-(long long)m - 1ll, 1ll));
    }
    // int_1^inf: e^-1 / (b_0 + a_1 / (b_1 + ...)), b_j = 2j + 2 - s, a_j = -j (j - s)
    SimRational tail = atom_value_less(atom_value_word(2ll * (long long)length, 1ll), s);
    for (unsigned int level = length - 1u; level > 0u; level -= 1u)
    {
        const SimRational below = atom_value_less(atom_value_word(2ll * (long long)level, 1ll), s);
        const SimRational above = atom_value_times(atom_value_word(-(long long)level, 1ll),
                                                   atom_value_less(atom_value_word((long long)level, 1ll), s));
        tail = atom_value_add(below, atom_value_over(above, tail));
    }
    return atom_value_add(lower, atom_value_over(atom_value_e(values, atom_value_word(-1ll, 1ll)), tail));
}

// scale times sum over m, k < length of (rise)_k 2^(-k) (-z)^m / (m! (first + m) (first + m + 1) ... (first + m + k)),
// the integrals int_0^1 e^(-zt) t^(first-1) (1 + t)^(-rise) dt with (1 + t)^(-rise) = 2^(-rise) times its series in
// (1 - t) / 2, scale holding 2^(-rise)
static SimRational atom_value_beta_sum(SimRational z, SimRational rise, SimRational first, SimRational scale,
                                       unsigned int length)
{
    SimRational sum = atom_value_word(0ll, 1ll);
    SimRational outer = atom_value_word(1ll, 1ll);
    for (unsigned int m = 0u; m < length; m += 1u)
    {
        const SimRational start = atom_value_add(first, atom_value_word((long long)m, 1ll));
        SimRational rising = atom_value_word(1ll, 1ll);
        SimRational product = start;
        for (unsigned int k = 0u; k < length; k += 1u)
        {
            sum = atom_value_add(sum, atom_value_times(outer, atom_value_over(rising, product)));
            rising = atom_value_times(rising, atom_value_over(atom_value_add(rise, atom_value_word((long long)k, 1ll)),
                                                              atom_value_word(2ll, 1ll)));
            product = atom_value_times(product, atom_value_add(start, atom_value_word((long long)k + 1ll, 1ll)));
        }
        outer = atom_value_times(outer, atom_value_over(z, atom_value_word(-(long long)m - 1ll, 1ll)));
    }
    return atom_value_times(scale, sum);
}

// e^(-z) sum_j weights[j] 2^(1-j) eps_j(2z): int_1^inf e^(-z t) sum_j weights[j] (1 + t)^(-j) dt
static SimRational atom_value_above(AtomValues *values, SimRational z, const std::vector<SimRational> &weights)
{
    const SimRational x = atom_value_times(atom_value_word(2ll, 1ll), z);
    SimRational sum = atom_value_word(0ll, 1ll);
    SimRational scale = atom_value_word(2ll, 1ll);
    for (size_t j = 0u; j < weights.size(); j += 1u)
    {
        if (sim_rational_sign(weights[j]) != 0)
        {
            sum = atom_value_add(sum, atom_value_times(weights[j], atom_value_times(scale, atom_value_ratio(values, x, (int)j))));
        }
        scale = atom_value_times(scale, atom_value_word(1ll, 2ll));
    }
    return atom_value_times(atom_value_e(values, sim_rational_negative(z)), sum);
}

// the product of two series in v, as many terms as the two give
static std::vector<SimRational> atom_value_product(const std::vector<SimRational> &left, const std::vector<SimRational> &right)
{
    std::vector<SimRational> product(left.size() + right.size() - 1u, atom_value_word(0ll, 1ll));
    for (size_t first = 0u; first < left.size(); first += 1u)
    {
        if (sim_rational_sign(left[first]) == 0)
        {
            continue;
        }
        for (size_t second = 0u; second < right.size(); second += 1u)
        {
            product[first + second] = atom_value_add(product[first + second], atom_value_times(left[first], right[second]));
        }
    }
    return product;
}

// v^shift (1 - v)^power, its coefficients
static std::vector<SimRational> atom_value_polynomial(unsigned int shift, unsigned int power)
{
    std::vector<SimRational> coefficients(shift + power + 1u, atom_value_word(0ll, 1ll));
    SimRational choose = atom_value_word(1ll, 1ll);
    for (unsigned int index = 0u; index <= power; index += 1u)
    {
        coefficients[shift + index] = choose;
        choose = atom_value_times(choose, atom_value_word(-((long long)power - (long long)index), (long long)index + 1ll));
    }
    return coefficients;
}

// sum over k < length of (a)_k / k! v^k, the series of (1 - v)^(-a)
static std::vector<SimRational> atom_value_binomial_series(SimRational a, unsigned int length)
{
    std::vector<SimRational> series(length, atom_value_word(0ll, 1ll));
    SimRational term = atom_value_word(1ll, 1ll);
    for (unsigned int k = 0u; k < length; k += 1u)
    {
        series[k] = term;
        term = atom_value_times(term, atom_value_over(atom_value_add(a, atom_value_word((long long)k, 1ll)),
                                                      atom_value_word((long long)k + 1ll, 1ll)));
    }
    return series;
}

void atom_value_integral(AtomValues *values, SimRational z, SimRational *value, SimRational *slope)
{
    const SimRational h = values->h;
    const SimRational one = atom_value_word(1ll, 1ll);
    const unsigned int length = values->length;
    const SimRational half_power = atom_value_power(values, atom_value_word(2ll, 1ll), sim_rational_negative(h));
    const SimRational below = atom_value_beta_sum(z, h, atom_value_add(h, one), half_power, length);
    const SimRational below_slope = atom_value_beta_sum(z, h, atom_value_add(h, atom_value_word(2ll, 1ll)), half_power, length);
    // t^h (1 + t)^(-h) = (1 - 1/(1 + t))^h, and t = (1 + t) - 1 for the slope
    const std::vector<SimRational> weights = atom_value_binomial_series(sim_rational_negative(h), length);
    // t sum_k c_k (1 + t)^(-k) = sum_j (c_(j+1) - c_j) (1 + t)^(-j) + c_0 (1 + t)
    std::vector<SimRational> lifted(length, atom_value_word(0ll, 1ll));
    for (unsigned int j = 0u; j < length; j += 1u)
    {
        const SimRational next = (j + 1u < length) ? weights[j + 1u] : atom_value_word(0ll, 1ll);
        lifted[j] = atom_value_less(next, weights[j]);
    }
    const SimRational above = atom_value_above(values, z, weights);
    const SimRational x = atom_value_times(atom_value_word(2ll, 1ll), z);
    SimRational above_slope = atom_value_above(values, z, lifted);
    // the k = 0 term, int_1^inf e^(-zt) (1 + t) dt = e^(-z) 4 eps_-1(2z)
    above_slope = atom_value_add(above_slope, atom_value_times(atom_value_e(values, sim_rational_negative(z)),
                                                               atom_value_times(atom_value_word(4ll, 1ll),
                                                                                atom_value_ratio(values, x, -1))));
    *value = atom_value_over(atom_value_add(below, above), values->gamma_once);
    *slope = sim_rational_negative(atom_value_over(atom_value_add(below_slope, above_slope), values->gamma_once));
}

void atom_value_open(AtomValues *values, SimRational h, unsigned int length)
{
    const SimRational one = atom_value_word(1ll, 1ll);
    values->h = h;
    values->length = length;
    values->exponentials.clear();
    values->ratios.clear();
    values->e = atom_value_exponential(one, length);
    values->gamma_once = atom_value_gamma(values, atom_value_add(one, h));
    values->gamma_twice = atom_value_gamma(values, atom_value_add(one, atom_value_times(atom_value_word(2ll, 1ll), h)));
    atom_value_integral(values, one, &values->value_at_one, &values->slope_at_one);
}

void atom_value_kummer(AtomValues *values, SimRational z, SimRational *value, SimRational *slope)
{
    const SimRational one = atom_value_word(1ll, 1ll);
    if (sim_rational_equal(z, one))
    {
        *value = values->value_at_one;
        *slope = values->slope_at_one;
        return;
    }
    const SimRational offset = sim_rational_absolute(atom_value_less(z, one));
    if (sim_rational_sign(atom_value_less(offset, one)) >= 0)
    {
        s_sim_rational_wide = 1;
    }
    // w's series about 1, atom 0 standing for w(1) and atom 1 for w'(1)
    const TaylorSeries series = ode_series_kummer(one, values->h, values->length, 0u, 1u);
    const AtomForm at = taylor_value(series, z);
    const AtomForm slope_at = taylor_value(taylor_derivative(series), z);
    *value = atom_value_add(atom_value_times(atom_form_coefficient_of(at, 0u), values->value_at_one),
                            atom_value_times(atom_form_coefficient_of(at, 1u), values->slope_at_one));
    *slope = atom_value_add(atom_value_times(atom_form_coefficient_of(slope_at, 0u), values->value_at_one),
                            atom_value_times(atom_form_coefficient_of(slope_at, 1u), values->slope_at_one));
}

SimRational atom_value_linear_tail(AtomValues *values, SimRational z_b)
{
    const SimRational h = values->h;
    SimRational value;
    SimRational slope;
    atom_value_kummer(values, z_b, &value, &slope);
    const SimRational reach = atom_value_times(z_b, atom_value_power(values, z_b, sim_rational_negative(h)));
    return atom_value_over(atom_value_add(atom_value_times(atom_value_times(z_b, z_b), atom_value_less(slope, value)), reach),
                           atom_value_less(atom_value_word(1ll, 1ll), h));
}

// sum_n D_n G_n^+ and sum_n D_n G_n, each over Gamma(2 + 2h)
static void atom_value_square_sums(AtomValues *values, SimRational z, SimRational *lifted_sum, SimRational *plain_sum)
{
    const SimRational h = values->h;
    const SimRational one = atom_value_word(1ll, 1ll);
    const SimRational twice_h = atom_value_times(atom_value_word(2ll, 1ll), h);
    const unsigned int length = values->length;
    // (h)_j (h + 1)_j / j!
    std::vector<SimRational> rising(length, one);
    for (unsigned int j = 0u; j + 1u < length; j += 1u)
    {
        const SimRational step = atom_value_times(atom_value_add(h, atom_value_word((long long)j, 1ll)),
                                                  atom_value_add(h, atom_value_word((long long)j + 1ll, 1ll)));
        rising[j + 1u] = atom_value_times(rising[j], atom_value_over(step, atom_value_word((long long)j + 1ll, 1ll)));
    }
    // (1 - v)^(2h - 1)
    const std::vector<SimRational> tail_series = atom_value_binomial_series(atom_value_less(one, twice_h), length);
    const SimRational half_power = atom_value_power(values, atom_value_word(2ll, 1ll), sim_rational_negative(h));
    SimRational scale = atom_value_times(half_power, half_power);
    SimRational pochhammer = one;
    SimRational lifted = atom_value_word(0ll, 1ll);
    SimRational plain = atom_value_word(0ll, 1ll);
    for (unsigned int n = 0u; n < length; n += 1u)
    {
        SimRational weight = atom_value_word(0ll, 1ll);
        for (unsigned int j = 0u; j <= n; j += 1u)
        {
            weight = atom_value_add(weight, atom_value_times(rising[j], rising[n - j]));
        }
        weight = atom_value_over(weight, pochhammer);
        const SimRational a = atom_value_add(twice_h, atom_value_word((long long)n, 1ll));
        const SimRational below = atom_value_beta_sum(z, a, a, scale, length);
        const SimRational below_lifted = atom_value_beta_sum(z, a, atom_value_add(a, one), scale, length);
        const SimRational above = atom_value_above(values, z, atom_value_product(atom_value_polynomial(1u, n), tail_series));
        const SimRational above_lifted =
            atom_value_above(values, z, atom_value_product(atom_value_polynomial(0u, n + 1u), tail_series));
        plain = atom_value_add(plain, atom_value_times(weight, atom_value_add(below, above)));
        lifted = atom_value_add(lifted, atom_value_times(weight, atom_value_add(below_lifted, above_lifted)));
        pochhammer = atom_value_times(pochhammer, atom_value_add(atom_value_add(twice_h, atom_value_word(2ll, 1ll)),
                                                                 atom_value_word((long long)n, 1ll)));
        scale = atom_value_times(scale, atom_value_word(1ll, 2ll));
    }
    const SimRational gamma = atom_value_times(atom_value_add(one, twice_h), values->gamma_twice);
    *lifted_sum = atom_value_over(lifted, gamma);
    *plain_sum = atom_value_over(plain, gamma);
}

SimRational atom_value_square_tail(AtomValues *values, SimRational z)
{
    const SimRational twice_h = atom_value_times(atom_value_word(2ll, 1ll), values->h);
    SimRational lifted;
    SimRational plain;
    atom_value_square_sums(values, z, &lifted, &plain);
    const SimRational reach = atom_value_power(values, z, sim_rational_negative(values->h));
    return atom_value_less(atom_value_add(atom_value_times(z, lifted), plain),
                           atom_value_over(atom_value_times(reach, reach), twice_h));
}

SimRational atom_value_square_integral(AtomValues *values, SimRational z)
{
    SimRational lifted;
    SimRational plain;
    atom_value_square_sums(values, z, &lifted, &plain);
    return lifted;
}

// the text from `start` up to `stop` read as p/q or p: 1, or 0 where it is not one
static int atom_value_read(const std::string &text, size_t start, size_t stop, SimRational *value)
{
    const std::string part = text.substr(start, stop - start);
    const char *const begin = part.c_str();
    char *end = NULL;
    const long long numerator = strtoll(begin, &end, 10);
    if (end == begin)
    {
        return 0;
    }
    long long denominator = 1ll;
    if (*end == '/')
    {
        const char *const after = end + 1;
        denominator = strtoll(after, &end, 10);
        if ((end == after) || (denominator <= 0ll))
        {
            return 0;
        }
    }
    if (*end != '\0')
    {
        return 0;
    }
    *value = atom_value_word(numerator, denominator);
    return 1;
}

// the rational between `prefix` at the start of the name and `suffix` at its end
static int atom_value_between(const std::string &name, const std::string &prefix, const std::string &suffix,
                              SimRational *value)
{
    if ((name.size() <= prefix.size() + suffix.size()) || (name.compare(0u, prefix.size(), prefix) != 0) ||
        (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0))
    {
        return 0;
    }
    return atom_value_read(name, prefix.size(), name.size() - suffix.size(), value);
}

// int_(R)^inf ((T)^(-1-h) x w(x/(T)) - x^(-h)) dx = T^(1-h) int_(R/T)^inf (z w - z^(-h)) dz
static int atom_value_heat_tail(AtomValues *values, const std::string &name, SimRational *value)
{
    const std::string open = "int_(";
    const std::string reach = ")^inf ((";
    const std::string power = ")^(-1-h) x w(x/(";
    const std::string close = ")) - x^(-h)) dx";
    if (name.compare(0u, open.size(), open) != 0)
    {
        return 0;
    }
    const size_t first = name.find(reach);
    if (first == std::string::npos)
    {
        return 0;
    }
    const size_t second = name.find(power, first + reach.size());
    if (second == std::string::npos)
    {
        return 0;
    }
    SimRational end;
    SimRational spread;
    const std::string spread_text = name.substr(first + reach.size(), second - first - reach.size());
    if (!atom_value_read(name, open.size(), first, &end) || !atom_value_read(spread_text, 0u, spread_text.size(), &spread) ||
        (name.substr(second) != power + spread_text + close))
    {
        return 0;
    }
    const SimRational lift = atom_value_times(spread, atom_value_power(values, spread, sim_rational_negative(values->h)));
    *value = atom_value_times(lift, atom_value_linear_tail(values, atom_value_over(end, spread)));
    return 1;
}

int atom_value_named(AtomValues *values, const std::string &name, SimRational *value)
{
    const SimRational h = values->h;
    SimRational x;
    if (name == "2^(-1/2)")
    {
        *value = atom_value_power(values, atom_value_word(2ll, 1ll), atom_value_word(-1ll, 2ll));
        return 1;
    }
    if (atom_value_between(name, "(", ")^(-h)", &x))
    {
        *value = atom_value_power(values, x, sim_rational_negative(h));
        return 1;
    }
    if (atom_value_between(name, "w(", ")", &x))
    {
        SimRational slope;
        atom_value_kummer(values, x, value, &slope);
        return 1;
    }
    if (atom_value_between(name, "w'(", ")", &x))
    {
        SimRational level;
        atom_value_kummer(values, x, &level, value);
        return 1;
    }
    if (atom_value_between(name, "E1(", ")", &x))
    {
        *value = atom_value_times(atom_value_e(values, sim_rational_negative(x)), atom_value_ratio(values, x, 1));
        return 1;
    }
    if (atom_value_between(name, "rho(", ")", &x))
    {
        const SimRational ratio = atom_value_less(sim_rational_reciprocal(x),
                                                  sim_rational_reciprocal(atom_value_less(atom_value_word(1ll, 1ll), x)));
        *value = sim_rational_reciprocal(atom_value_add(atom_value_word(1ll, 1ll), atom_value_e(values, ratio)));
        return 1;
    }
    if (atom_value_between(name, "int_(", ")^inf (z w^2 - z^(-1-2h)) dz", &x))
    {
        *value = atom_value_square_tail(values, x);
        return 1;
    }
    if (atom_value_between(name, "int_(", ")^inf w^2", &x))
    {
        *value = atom_value_square_integral(values, x);
        return 1;
    }
    return atom_value_heat_tail(values, name, value);
}

SimRational atom_value_form(AtomValues *values, const AtomForm &form, const std::vector<SimRational> &atoms)
{
    // atoms[i]^(k+1) at raised[i][k], as the form asks for them
    std::vector<std::vector<SimRational>> raised(atoms.size());
    SimRational sum = atom_value_word(0ll, 1ll);
    for (unsigned int slot : atom_form_slots(form))
    {
        const AtomKey &key = atom_form_key_at(slot);
        SimRational term = atom_form_coefficient_at(form, slot);
        if (sim_rational_sign(key.e) != 0)
        {
            term = atom_value_times(term, atom_value_e(values, key.e));
        }
        for (size_t atom = 0u; atom < key.power.size(); atom += 1u)
        {
            const unsigned int power = key.power[atom];
            if (power == 0u)
            {
                continue;
            }
            if (atom >= atoms.size())
            {
                s_sim_rational_wide = 1;
                continue;
            }
            std::vector<SimRational> &powers = raised[atom];
            while (powers.size() < power)
            {
                powers.push_back(powers.empty() ? atoms[atom] : atom_value_times(powers.back(), atoms[atom]));
            }
            term = atom_value_times(term, powers[power - 1u]);
        }
        sum = atom_value_add(sum, term);
    }
    return sum;
}

int atom_value_short(void)
{
    return s_sim_rational_wide != 0;
}
