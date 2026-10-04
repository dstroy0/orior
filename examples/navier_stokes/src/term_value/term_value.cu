// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// term_value.cu: the terms' values as the sums and chains the record machine runs (term_value.h)
#include "term_value.h"

#include <stdlib.h>

static int s_term_value_short = 0;

static unsigned long long term_value_bit_length(unsigned long long value)
{
    unsigned long long bits = 0ull;
    while (value != 0ull)
    {
        bits += 1ull;
        value >>= 1u;
    }
    return bits;
}

// the bits a sum of `count` rows adds to its widest row
static unsigned long long term_value_count_bits(unsigned long long count)
{
    return (count <= 1ull) ? 0ull : term_value_bit_length(count - 1ull);
}

// the primes of `number`, a whole number above 0, joined `times` times over
static void term_value_factor(unsigned long long number, unsigned int times, TermValuePrimes *primes)
{
    if (times == 0u)
    {
        return;
    }
    for (unsigned long long prime = 2ull; prime * prime <= number; prime += (prime == 2ull) ? 1ull : 2ull)
    {
        while (number % prime == 0ull)
        {
            (*primes)[prime] += times;
            number /= prime;
        }
    }
    if (number > 1ull)
    {
        (*primes)[number] += times;
    }
}

std::vector<unsigned long long> term_value_words(const TermValuePrimes &primes)
{
    std::vector<unsigned long long> words;
    unsigned long long word = 1ull;
    for (const auto &entry : primes)
    {
        for (unsigned int count = 0u; count < entry.second; count += 1u)
        {
            if (word > (1ull << 62u) / entry.first)
            {
                words.push_back(word);
                word = 1ull;
            }
            word *= entry.first;
        }
    }
    if ((word != 1ull) || words.empty())
    {
        words.push_back(word);
    }
    return words;
}

static unsigned long long term_value_primes_bits(const TermValuePrimes &primes)
{
    unsigned long long bits = 0ull;
    for (unsigned long long word : term_value_words(primes))
    {
        bits += term_value_bit_length(word);
    }
    return bits;
}

static unsigned long long term_value_wide_bits(const std::vector<unsigned int> &wide)
{
    for (size_t limb = wide.size(); limb > 0u; limb -= 1u)
    {
        if (wide[limb - 1u] != 0u)
        {
            return 32ull * (limb - 1u) + term_value_bit_length(wide[limb - 1u]);
        }
    }
    return 0ull;
}

unsigned long long term_value_row_bits(const TermValues *values, const TermValueRow &row)
{
    unsigned long long bits = term_value_primes_bits(row.primes) + term_value_wide_bits(row.wide);
    for (const auto &entry : row.residues)
    {
        bits += (unsigned long long)entry.second * values->residues[entry.first].bits;
    }
    return bits;
}

static unsigned int term_value_row_level(const TermValues *values, const TermValueRow &row)
{
    unsigned int level = 0u;
    for (const auto &entry : row.residues)
    {
        level = (values->residues[entry.first].level > level) ? values->residues[entry.first].level : level;
    }
    return level;
}

static unsigned int term_value_residue(TermValues *values, unsigned long long bits, unsigned int level)
{
    TermValueResidue residue;
    residue.bits = (bits == 0ull) ? 1ull : bits;
    residue.level = level;
    values->residues.push_back(residue);
    return (unsigned int)values->residues.size() - 1u;
}

// the most residues a row raises past the first power, each raised by its own steps in the lane
static const size_t s_term_value_raised_most = 4u;

static unsigned int term_value_sum(TermValues *values, const std::vector<TermValueRow> &given);

// the row with its raised residues past the most taken first into a product of their own, the row taking it once
static TermValueRow term_value_row_short(TermValues *values, const TermValueRow &given)
{
    TermValueRow row = given;
    for (;;)
    {
        TermValueRow part;
        part.negative = 0;
        size_t raised = 0u;
        for (const auto &entry : row.residues)
        {
            raised += (entry.second > 1u) ? 1u : 0u;
        }
        if (raised <= s_term_value_raised_most)
        {
            return row;
        }
        for (auto entry = row.residues.begin(); (entry != row.residues.end()) && (part.residues.size() < s_term_value_raised_most);)
        {
            if (entry->second > 1u)
            {
                part.residues[entry->first] = entry->second;
                entry = row.residues.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
        row.residues[term_value_sum(values, {part})] += 1u;
    }
}

// the residue the rows sum to, written at the sweep after the last residue they read
static unsigned int term_value_sum(TermValues *values, const std::vector<TermValueRow> &given)
{
    std::vector<TermValueRow> rows;
    for (const TermValueRow &row : given)
    {
        rows.push_back(term_value_row_short(values, row));
    }
    unsigned long long bits = 0ull;
    unsigned int level = 0u;
    for (const TermValueRow &row : rows)
    {
        const unsigned long long row_bits = term_value_row_bits(values, row);
        bits = (row_bits > bits) ? row_bits : bits;
        const unsigned int row_level = term_value_row_level(values, row);
        level = (row_level > level) ? row_level : level;
    }
    const unsigned int target = term_value_residue(values, bits + term_value_count_bits(rows.size()), level + 1u);
    TermValueSum sum;
    sum.target = target;
    sum.rows = rows;
    values->sums.push_back(sum);
    return target;
}

static void term_value_join(TermValuePrimes *into, const TermValuePrimes &from)
{
    for (const auto &entry : from)
    {
        (*into)[entry.first] += entry.second;
    }
}

static void term_value_join_powers(TermValuePowers *into, const TermValuePowers &from)
{
    for (const auto &entry : from)
    {
        (*into)[entry.first] += entry.second;
    }
}

// the row times the other: signs, factors and residues joined; at most one of the two holds a wide factor
static TermValueRow term_value_row_times(const TermValueRow &left, const TermValueRow &right)
{
    TermValueRow row = left;
    row.negative = left.negative ^ right.negative;
    term_value_join(&row.primes, right.primes);
    term_value_join_powers(&row.residues, right.residues);
    if (!right.wide.empty())
    {
        row.wide = right.wide;
    }
    return row;
}

TermValue term_value_whole(long long numerator, long long denominator)
{
    TermValue value;
    if (numerator == 0ll)
    {
        return value;
    }
    TermValueRow row;
    row.negative = (numerator < 0ll) != (denominator < 0ll);
    term_value_factor((unsigned long long)((numerator < 0ll) ? -numerator : numerator), 1u, &row.primes);
    term_value_factor((unsigned long long)((denominator < 0ll) ? -denominator : denominator), 1u, &value.divisor_primes);
    value.rows.push_back(row);
    return value;
}

TermValue term_value_from(SimRational rational)
{
    long long numerator = 0ll;
    long long denominator = 1ll;
    if (!sim_rational_small(&rational.numerator, &numerator) || !sim_rational_small(&rational.denominator, &denominator) ||
        (denominator == 0ll))
    {
        s_term_value_short = 1;
        return TermValue();
    }
    return term_value_whole(numerator, denominator);
}

// every row times numerator / denominator
static void term_value_scale(TermValue *value, long long numerator, long long denominator)
{
    if ((numerator == 0ll) || value->rows.empty())
    {
        value->rows.clear();
        return;
    }
    for (TermValueRow &row : value->rows)
    {
        row.negative ^= (numerator < 0ll) != (denominator < 0ll);
        term_value_factor((unsigned long long)((numerator < 0ll) ? -numerator : numerator), 1u, &row.primes);
    }
    term_value_factor((unsigned long long)((denominator < 0ll) ? -denominator : denominator), 1u, &value->divisor_primes);
}

static TermValue term_value_negative(const TermValue &value)
{
    TermValue negative = value;
    for (TermValueRow &row : negative.rows)
    {
        row.negative ^= 1;
    }
    return negative;
}

// the sum of every value, over one divisor each divisor divides: each prime and each residue at its highest power
static TermValue term_value_add_all(const std::vector<TermValue> &parts)
{
    TermValue sum;
    for (const TermValue &part : parts)
    {
        if (part.rows.empty())
        {
            continue;
        }
        for (const auto &entry : part.divisor_primes)
        {
            unsigned int &held = sum.divisor_primes[entry.first];
            held = (entry.second > held) ? entry.second : held;
        }
        for (const auto &entry : part.divisor_residues)
        {
            unsigned int &held = sum.divisor_residues[entry.first];
            held = (entry.second > held) ? entry.second : held;
        }
    }
    for (const TermValue &part : parts)
    {
        if (part.rows.empty())
        {
            continue;
        }
        TermValueRow lift;
        lift.negative = 0;
        for (const auto &entry : sum.divisor_primes)
        {
            const auto found = part.divisor_primes.find(entry.first);
            const unsigned int held = (found == part.divisor_primes.end()) ? 0u : found->second;
            if (entry.second > held)
            {
                lift.primes[entry.first] = entry.second - held;
            }
        }
        for (const auto &entry : sum.divisor_residues)
        {
            const auto found = part.divisor_residues.find(entry.first);
            const unsigned int held = (found == part.divisor_residues.end()) ? 0u : found->second;
            if (entry.second > held)
            {
                lift.residues[entry.first] = entry.second - held;
            }
        }
        for (const TermValueRow &row : part.rows)
        {
            sum.rows.push_back(term_value_row_times(row, lift));
        }
    }
    return sum;
}

TermValue term_value_add(TermValues *values, const TermValue &left, const TermValue &right)
{
    (void)values;
    return term_value_add_all({left, right});
}

TermValue term_value_less(TermValues *values, const TermValue &left, const TermValue &right)
{
    (void)values;
    return term_value_add_all({left, term_value_negative(right)});
}

TermValue term_value_written(TermValues *values, const TermValue &value)
{
    if (value.rows.empty())
    {
        return value;
    }
    if (value.rows.size() == 1u)
    {
        const TermValueRow &row = value.rows[0];
        if (!row.negative && row.primes.empty() && row.wide.empty() && (row.residues.size() == 1u) &&
            (row.residues.begin()->second == 1u))
        {
            return value;
        }
    }
    TermValue written;
    written.divisor_primes = value.divisor_primes;
    written.divisor_residues = value.divisor_residues;
    TermValueRow row;
    row.negative = 0;
    row.residues[term_value_sum(values, value.rows)] = 1u;
    written.rows.push_back(row);
    return written;
}

TermValue term_value_times(TermValues *values, const TermValue &left, const TermValue &right)
{
    if (left.rows.empty() || right.rows.empty())
    {
        return TermValue();
    }
    // a product of many rows by many is written first: no product spreads past a few rows
    if (left.rows.size() * right.rows.size() > 8u)
    {
        if (left.rows.size() >= right.rows.size())
        {
            return term_value_times(values, term_value_written(values, left), right);
        }
        return term_value_times(values, left, term_value_written(values, right));
    }
    int wide_left = 0;
    int wide_right = 0;
    for (const TermValueRow &row : left.rows)
    {
        wide_left = wide_left || !row.wide.empty();
    }
    for (const TermValueRow &row : right.rows)
    {
        wide_right = wide_right || !row.wide.empty();
    }
    if (wide_left && wide_right)
    {
        return term_value_times(values, term_value_written(values, left), right);
    }
    TermValue product;
    product.divisor_primes = left.divisor_primes;
    product.divisor_residues = left.divisor_residues;
    term_value_join(&product.divisor_primes, right.divisor_primes);
    term_value_join_powers(&product.divisor_residues, right.divisor_residues);
    for (const TermValueRow &first : left.rows)
    {
        for (const TermValueRow &second : right.rows)
        {
            product.rows.push_back(term_value_row_times(first, second));
        }
    }
    return product;
}

TermValue term_value_reciprocal(TermValues *values, const TermValue &value)
{
    if (value.rows.empty())
    {
        s_term_value_short = 1;
        return value;
    }
    const TermValue single =
        ((value.rows.size() == 1u) && value.rows[0].wide.empty()) ? value : term_value_written(values, value);
    const TermValueRow &row = single.rows[0];
    TermValue reciprocal;
    TermValueRow top;
    top.negative = row.negative;
    top.primes = single.divisor_primes;
    top.residues = single.divisor_residues;
    reciprocal.rows.push_back(top);
    reciprocal.divisor_primes = row.primes;
    reciprocal.divisor_residues = row.residues;
    return reciprocal;
}

// base^power for a whole power, the reciprocal's where the power is negative
static TermValue term_value_raised_value(TermValues *values, const TermValue &base, long long power)
{
    TermValue value = term_value_whole(1ll, 1ll);
    const long long steps = (power < 0ll) ? -power : power;
    for (long long step = 0ll; step < steps; step += 1ll)
    {
        value = term_value_times(values, value, base);
    }
    return (power < 0ll) ? term_value_reciprocal(values, value) : value;
}

// (n/d)^power as whole factors, for parts past a word once raised
static TermValue term_value_rational_power(long long numerator, long long denominator, unsigned int power)
{
    TermValue value;
    if ((numerator == 0ll) && (power > 0u))
    {
        return value;
    }
    TermValueRow row;
    row.negative = (power % 2u == 1u) && ((numerator < 0ll) != (denominator < 0ll));
    if (power > 0u)
    {
        term_value_factor((unsigned long long)((numerator < 0ll) ? -numerator : numerator), power, &row.primes);
        term_value_factor((unsigned long long)((denominator < 0ll) ? -denominator : denominator), power,
                          &value.divisor_primes);
    }
    value.rows.push_back(row);
    return value;
}

static unsigned long long term_value_word_bits(long long word)
{
    return term_value_bit_length((unsigned long long)((word < 0ll) ? -word : word));
}

// the bits of left x + right y: a term with a zero factor or a zero side drops out
static unsigned long long term_value_step_bits(long long left, unsigned long long left_bits, long long right,
                                               unsigned long long right_bits)
{
    const unsigned long long first = ((left == 0ll) || (left_bits == 0ull)) ? 0ull : term_value_word_bits(left) + left_bits;
    const unsigned long long second =
        ((right == 0ll) || (right_bits == 0ull)) ? 0ull : term_value_word_bits(right) + right_bits;
    if (first == 0ull)
    {
        return second;
    }
    if (second == 0ull)
    {
        return first;
    }
    return ((first > second) ? first : second) + 1ull;
}

// the chains of one run written: X times each tail summed over the run, Y beside it for a run of one, and X after every
// step where `keep` asks
static void term_value_run(TermValues *values, std::vector<TermValueChain> chains, int keep, unsigned int *x_target,
                           unsigned int *y_target)
{
    unsigned long long x_most = 0ull;
    unsigned long long y_most = 0ull;
    unsigned int level = 0u;
    std::vector<std::vector<unsigned long long>> history_bits(chains.size());
    for (size_t at = 0u; at < chains.size(); at += 1u)
    {
        const TermValueChain &chain = chains[at];
        unsigned long long x_bits = term_value_step_bits(chain.x_word, values->residues[chain.x_residue].bits, 0ll, 0ull);
        unsigned long long y_bits = term_value_step_bits(chain.y_word, values->residues[chain.y_residue].bits, 0ll, 0ull);
        for (const TermValueStep &step : chain.steps)
        {
            const unsigned long long x_next = term_value_step_bits(step.a, x_bits, step.b, y_bits);
            const unsigned long long y_next = term_value_step_bits(step.c, x_bits, step.d, y_bits);
            x_bits = x_next;
            y_bits = y_next;
            history_bits[at].push_back(x_bits);
        }
        const unsigned long long tail_bits = x_bits + term_value_row_bits(values, chain.tail);
        x_most = (tail_bits > x_most) ? tail_bits : x_most;
        y_most = (y_bits > y_most) ? y_bits : y_most;
        const unsigned int x_level = values->residues[chain.x_residue].level;
        const unsigned int y_level = values->residues[chain.y_residue].level;
        level = (x_level > level) ? x_level : level;
        level = (y_level > level) ? y_level : level;
    }
    TermValueRun run;
    run.x_target = term_value_residue(values, x_most + term_value_count_bits(chains.size()), level + 1u);
    run.y_target = term_value_residue(values, y_most, level + 1u);
    for (size_t at = 0u; keep && (at < chains.size()); at += 1u)
    {
        for (unsigned long long bits : history_bits[at])
        {
            chains[at].history.push_back(term_value_residue(values, bits, level + 1u));
        }
    }
    run.chains = chains;
    values->runs.push_back(run);
    *x_target = run.x_target;
    if (y_target != NULL)
    {
        *y_target = run.y_target;
    }
}

// a chain from X = x_word, Y = y_word
static TermValueChain term_value_chain(long long x_word, long long y_word)
{
    TermValueChain chain;
    chain.x_word = x_word;
    chain.x_residue = 0u;
    chain.y_word = y_word;
    chain.y_residue = 0u;
    chain.tail.negative = 0;
    return chain;
}

static void term_value_step(TermValueChain *chain, long long a, long long b, long long c, long long d)
{
    TermValueStep step = {a, b, c, d};
    chain->steps.push_back(step);
}

// sum over k < length of prod_(i<k) top(i) / bottom(i) by Horner's rule, from i = length - 2 down: X over
// prod bottom(i), with top(i) = top_whole + top_step i and bottom(i) = (bottom_whole + bottom_step i) (i + 1)
static TermValue term_value_horner(TermValues *values, long long top_whole, long long top_step, long long bottom_whole,
                                   long long bottom_step)
{
    const unsigned int length = values->length;
    TermValueChain chain = term_value_chain(1ll, 1ll);
    TermValue value;
    for (unsigned int index = length - 1u; index > 0u; index -= 1u)
    {
        const long long at = (long long)index - 1ll;
        const long long top = top_whole + top_step * at;
        const long long bottom = (bottom_whole + bottom_step * at) * (at + 1ll);
        term_value_step(&chain, top, bottom, 0ll, bottom);
        term_value_factor((unsigned long long)bottom, 1u, &value.divisor_primes);
    }
    unsigned int x_target = 0u;
    term_value_run(values, {chain}, 0, &x_target, NULL);
    TermValueRow row;
    row.negative = 0;
    row.residues[x_target] = 1u;
    value.rows.push_back(row);
    return value;
}

static int term_value_parts_small(SimRational value, long long *top, long long *bottom)
{
    if (!sim_rational_small(&value.numerator, top) || !sim_rational_small(&value.denominator, bottom))
    {
        s_term_value_short = 1;
        return 0;
    }
    return 1;
}

// sum over k < length of x^k / k!
static TermValue term_value_exponential(TermValues *values, SimRational x)
{
    long long top = 0ll;
    long long bottom = 1ll;
    if (!term_value_parts_small(x, &top, &bottom))
    {
        return TermValue();
    }
    return term_value_horner(values, top, 0ll, bottom, 0ll);
}

// sum over k < length of (a)_k / k! y^k, the series of (1 - y)^(-a)
static TermValue term_value_binomial(TermValues *values, SimRational y, SimRational a)
{
    long long y_top = 0ll;
    long long y_bottom = 1ll;
    long long a_top = 0ll;
    long long a_bottom = 1ll;
    if (!term_value_parts_small(y, &y_top, &y_bottom) || !term_value_parts_small(a, &a_top, &a_bottom))
    {
        return TermValue();
    }
    if (y_top == 0ll)
    {
        return term_value_whole(1ll, 1ll);
    }
    // (a + i) y / (i + 1) = (a_top + i a_bottom) y_top / (a_bottom y_bottom (i + 1))
    return term_value_horner(values, a_top * y_top, a_bottom * y_top, a_bottom * y_bottom, 0ll);
}

// the largest whole number at or below q: 1, or 0 where q passes a word
static int term_value_floor(SimRational q, long long *whole)
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

TermValue term_value_e(TermValues *values, SimRational q)
{
    for (const TermValuePair &pair : values->exponentials)
    {
        if (sim_rational_equal(pair.key, q))
        {
            return pair.value;
        }
    }
    long long whole = 0ll;
    if (!term_value_floor(q, &whole))
    {
        s_term_value_short = 1;
        return TermValue();
    }
    const SimRational part = sim_rational_difference(q, sim_rational(whole, 1ll));
    TermValuePair pair;
    pair.key = q;
    pair.value = term_value_raised_value(values, values->e, whole);
    if (sim_rational_sign(part) != 0)
    {
        pair.value = term_value_times(values, pair.value, term_value_exponential(values, part));
    }
    values->exponentials.push_back(pair);
    return pair.value;
}

TermValue term_value_power(TermValues *values, SimRational x, SimRational p)
{
    for (const TermValuePower &power : values->powers)
    {
        if (sim_rational_equal(power.x, x) && sim_rational_equal(power.p, p))
        {
            return power.value;
        }
    }
    // |1 - 1/x| < 1 exactly where 2x - 1 > 0
    if (sim_rational_sign(sim_rational_difference(sim_rational_product(sim_rational(2ll, 1ll), x), sim_rational(1ll, 1ll))) <= 0)
    {
        s_term_value_short = 1;
        return TermValue();
    }
    const SimRational y = sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_reciprocal(x));
    TermValuePower power;
    power.x = x;
    power.p = p;
    power.value = term_value_binomial(values, y, p);
    values->powers.push_back(power);
    return power.value;
}

// the ratios held at x, eps_-1, eps_0 and eps_1 written where none are
static TermValueRatios *term_value_ratios(TermValues *values, SimRational x)
{
    for (TermValueRatios &ratios : values->ratios)
    {
        if (sim_rational_equal(ratios.x, x))
        {
            return &ratios;
        }
    }
    long long top = 0ll;
    long long bottom = 1ll;
    if (!term_value_parts_small(x, &top, &bottom) || (top <= 0ll))
    {
        s_term_value_short = 1;
        return NULL;
    }
    const unsigned int length = values->length;
    TermValueRatios ratios;
    ratios.x = x;
    // eps_-1 = 1/x + 1/x^2
    ratios.eps.push_back(term_value_add(values, term_value_whole(bottom, top), term_value_rational_power(bottom, top, 2u)));
    ratios.eps.push_back(term_value_whole(bottom, top));
    // eps_1 to `length` levels: the tail P/Q from x + 2 length - 1, then (x + 2l - 1) - l^2 / tail down to l = 1
    TermValueChain chain = term_value_chain(top + (2ll * (long long)length - 1ll) * bottom, bottom);
    for (unsigned int level = length - 1u; level > 0u; level -= 1u)
    {
        const long long at = (long long)level;
        term_value_step(&chain, top + (2ll * at - 1ll) * bottom, -at * at * bottom, bottom, 0ll);
    }
    term_value_run(values, {chain}, 0, &ratios.continued_x, &ratios.continued_y);
    TermValue first;
    TermValueRow row;
    row.negative = 0;
    row.residues[ratios.continued_y] = 1u;
    first.rows.push_back(row);
    first.divisor_residues[ratios.continued_x] = 1u;
    ratios.eps.push_back(first);
    values->ratios.push_back(ratios);
    return &values->ratios.back();
}

TermValue term_value_ratio(TermValues *values, SimRational x, int n)
{
    TermValueRatios *ratios = term_value_ratios(values, x);
    if ((ratios == NULL) || (n < -1))
    {
        s_term_value_short = 1;
        return TermValue();
    }
    if ((long long)ratios->eps.size() < (long long)n + 2ll)
    {
        if (ratios->eps.size() > 3u)
        {
            s_term_value_short = 1;
            return TermValue();
        }
        long long top = 0ll;
        long long bottom = 1ll;
        term_value_parts_small(x, &top, &bottom);
        const unsigned int continued_x = ratios->continued_x;
        // eps_m = N_m / (P bottom^(m-1) (m-1)!), N_m = C_m - top N_(m-1), C_m = P bottom^(m-1) (m-2)!, from N_1 = Q:
        // X = N_(m-1) and Y = C_m step to N_m and C_(m+1)
        const unsigned int last = ((unsigned int)n > 2u * values->length - 1u) ? (unsigned int)n : 2u * values->length - 1u;
        TermValueChain chain = term_value_chain(1ll, bottom);
        chain.x_residue = ratios->continued_y;
        chain.y_residue = continued_x;
        for (unsigned int order = 2u; order <= last; order += 1u)
        {
            term_value_step(&chain, -top, 1ll, 0ll, bottom * ((long long)order - 1ll));
        }
        unsigned int x_target = 0u;
        const size_t run = values->runs.size();
        term_value_run(values, {chain}, 1, &x_target, NULL);
        const std::vector<unsigned int> history = values->runs[run].chains[0].history;
        TermValuePrimes divisor;
        for (unsigned int order = 2u; order <= last; order += 1u)
        {
            term_value_factor((unsigned long long)bottom, 1u, &divisor);
            term_value_factor((unsigned long long)order - 1ull, 1u, &divisor);
            TermValue eps;
            TermValueRow row;
            row.negative = 0;
            row.residues[history[order - 2u]] = 1u;
            eps.rows.push_back(row);
            eps.divisor_primes = divisor;
            eps.divisor_residues[continued_x] = 1u;
            ratios->eps.push_back(eps);
        }
    }
    return ratios->eps[(size_t)(n + 1)];
}

TermValue term_value_gamma(TermValues *values, SimRational s)
{
    const unsigned int length = values->length;
    long long top = 0ll;
    long long bottom = 1ll;
    if (!term_value_parts_small(s, &top, &bottom))
    {
        return TermValue();
    }
    // int_0^1 e^(-t) t^(s-1) dt = (1/s) sum_m prod_(i<m) -(s + i) / ((i + 1)(s + i + 1))
    TermValue lower = term_value_horner(values, -top, -bottom, top + bottom, bottom);
    term_value_scale(&lower, bottom, top);
    // int_1^inf: e^-1 / tail, the tail P/Q from 2 length - s, then (2l - s) - l (l - s) / tail down to l = 1
    TermValueChain chain = term_value_chain(2ll * (long long)length * bottom - top, bottom);
    for (unsigned int level = length - 1u; level > 0u; level -= 1u)
    {
        const long long at = (long long)level;
        term_value_step(&chain, 2ll * at * bottom - top, -at * (at * bottom - top), bottom, 0ll);
    }
    unsigned int x_target = 0u;
    unsigned int y_target = 0u;
    term_value_run(values, {chain}, 0, &x_target, &y_target);
    TermValue inverse;
    TermValueRow row;
    row.negative = 0;
    row.residues[y_target] = 1u;
    inverse.rows.push_back(row);
    inverse.divisor_residues[x_target] = 1u;
    const TermValue tail = term_value_times(values, term_value_e(values, sim_rational(-1ll, 1ll)), inverse);
    return term_value_written(values, term_value_add(values, lower, tail));
}

// the divisor of the beta sum's run m: z_bottom^m m! (2 rise_bottom)^(length-1) prod_(j=m)^(m+length-1) g_j,
// g_j = first_top + j first_bottom
static TermValuePrimes term_value_beta_bottom(unsigned int m, unsigned int length, long long z_bottom, long long rise_bottom,
                                              long long first_top, long long first_bottom)
{
    TermValuePrimes primes;
    term_value_factor((unsigned long long)z_bottom, m, &primes);
    for (unsigned int at = 2u; at <= m; at += 1u)
    {
        term_value_factor(at, 1u, &primes);
    }
    term_value_factor((unsigned long long)(2ll * rise_bottom), length - 1u, &primes);
    for (unsigned int at = m; at < m + length; at += 1u)
    {
        term_value_factor((unsigned long long)(first_top + (long long)at * first_bottom), 1u, &primes);
    }
    return primes;
}

// every run's divisor of the beta sum joined into `divisor`, each prime at its highest power
static void term_value_beta_divisor(TermValues *values, SimRational z, SimRational rise, SimRational first,
                                    TermValuePrimes *divisor)
{
    long long z_top = 0ll;
    long long z_bottom = 1ll;
    long long rise_top = 0ll;
    long long rise_bottom = 1ll;
    long long first_top = 0ll;
    long long first_bottom = 1ll;
    if (!term_value_parts_small(z, &z_top, &z_bottom) || !term_value_parts_small(rise, &rise_top, &rise_bottom) ||
        !term_value_parts_small(first, &first_top, &first_bottom))
    {
        return;
    }
    for (unsigned int m = 0u; m < values->length; m += 1u)
    {
        for (const auto &entry : term_value_beta_bottom(m, values->length, z_bottom, rise_bottom, first_top, first_bottom))
        {
            unsigned int &held = (*divisor)[entry.first];
            held = (entry.second > held) ? entry.second : held;
        }
    }
}

// scale times sum over m, k < length of (rise)_k 2^(-k) (-z)^m / (m! (first + m) (first + m + 1) ... (first + m + k)),
// over `divisor`, which every run's divisor divides. Run m sums over k by Horner's rule:
// sum_k prod_(i<k) (rise + i) / (2 (first + m + i + 1)) = X_m / prod_i 2 rise_bottom g_(m+i+1), and its term is
// (-z_top)^m first_bottom X_m / (z_bottom^m m! g_m prod_i 2 rise_bottom g_(m+i+1)).
static TermValue term_value_beta_sum(TermValues *values, SimRational z, SimRational rise, SimRational first,
                                     const TermValue &scale, const TermValuePrimes &divisor)
{
    const unsigned int length = values->length;
    long long z_top = 0ll;
    long long z_bottom = 1ll;
    long long rise_top = 0ll;
    long long rise_bottom = 1ll;
    long long first_top = 0ll;
    long long first_bottom = 1ll;
    if (!term_value_parts_small(z, &z_top, &z_bottom) || !term_value_parts_small(rise, &rise_top, &rise_bottom) ||
        !term_value_parts_small(first, &first_top, &first_bottom))
    {
        return TermValue();
    }
    std::vector<TermValueChain> chains;
    for (unsigned int m = 0u; m < length; m += 1u)
    {
        TermValueChain chain = term_value_chain(1ll, 1ll);
        for (unsigned int index = length - 1u; index > 0u; index -= 1u)
        {
            const long long at = (long long)index - 1ll;
            const long long top = (rise_top + at * rise_bottom) * first_bottom;
            const long long bottom = 2ll * rise_bottom * (first_top + ((long long)m + at + 1ll) * first_bottom);
            term_value_step(&chain, top, bottom, 0ll, bottom);
        }
        chain.tail.negative = (m % 2u == 1u) && (z_top > 0ll);
        term_value_factor((unsigned long long)((z_top < 0ll) ? -z_top : z_top), m, &chain.tail.primes);
        term_value_factor((unsigned long long)first_bottom, 1u, &chain.tail.primes);
        const TermValuePrimes bottom = term_value_beta_bottom(m, length, z_bottom, rise_bottom, first_top, first_bottom);
        for (const auto &entry : bottom)
        {
            const auto found = divisor.find(entry.first);
            if ((found == divisor.end()) || (found->second < entry.second))
            {
                s_term_value_short = 1;
            }
        }
        for (const auto &entry : divisor)
        {
            const auto found = bottom.find(entry.first);
            const unsigned int held = (found == bottom.end()) ? 0u : found->second;
            if (entry.second > held)
            {
                chain.tail.primes[entry.first] += entry.second - held;
            }
        }
        chains.push_back(chain);
    }
    unsigned int x_target = 0u;
    term_value_run(values, chains, 0, &x_target, NULL);
    TermValue sum;
    TermValueRow row;
    row.negative = 0;
    row.residues[x_target] = 1u;
    sum.rows.push_back(row);
    sum.divisor_primes = divisor;
    return term_value_times(values, scale, sum);
}

// sum over j of weights[j] 2^(1-j) eps_j(2z), e^(-z) left to the caller
static TermValue term_value_above_sum(TermValues *values, SimRational z, const std::vector<TermValue> &weights)
{
    const SimRational x = sim_rational_product(sim_rational(2ll, 1ll), z);
    std::vector<TermValue> parts;
    for (size_t j = 0u; j < weights.size(); j += 1u)
    {
        if (weights[j].rows.empty())
        {
            continue;
        }
        TermValue scale = term_value_rational_power(1ll, 2ll, (unsigned int)j);
        term_value_scale(&scale, 2ll, 1ll);
        parts.push_back(term_value_times(values, term_value_times(values, weights[j], scale), term_value_ratio(values, x, (int)j)));
    }
    return term_value_written(values, term_value_add_all(parts));
}

// sum over k < length of (a)_k / k! v^k, the series of (1 - v)^(-a), each coefficient in whole factors
static std::vector<TermValue> term_value_binomial_series(SimRational a, unsigned int length)
{
    long long top = 0ll;
    long long bottom = 1ll;
    std::vector<TermValue> series;
    if (!term_value_parts_small(a, &top, &bottom))
    {
        return series;
    }
    TermValue term = term_value_whole(1ll, 1ll);
    for (unsigned int k = 0u; k < length; k += 1u)
    {
        series.push_back(term);
        term_value_scale(&term, top + (long long)k * bottom, bottom * ((long long)k + 1ll));
    }
    return series;
}

void term_value_integral(TermValues *values, SimRational z, TermValue *value, TermValue *slope)
{
    const SimRational h = values->h;
    const SimRational one = sim_rational(1ll, 1ll);
    const SimRational two = sim_rational(2ll, 1ll);
    const unsigned int length = values->length;
    const TermValue half_power = term_value_power(values, two, sim_rational_negative(h));
    TermValuePrimes below_divisor;
    term_value_beta_divisor(values, z, h, sim_rational_sum(h, one), &below_divisor);
    TermValuePrimes slope_divisor;
    term_value_beta_divisor(values, z, h, sim_rational_sum(h, two), &slope_divisor);
    const TermValue below = term_value_beta_sum(values, z, h, sim_rational_sum(h, one), half_power, below_divisor);
    const TermValue below_slope = term_value_beta_sum(values, z, h, sim_rational_sum(h, two), half_power, slope_divisor);
    // t^h (1 + t)^(-h) = (1 - 1/(1 + t))^h, and t = (1 + t) - 1 for the slope
    const std::vector<TermValue> weights = term_value_binomial_series(sim_rational_negative(h), length);
    // t sum_k c_k (1 + t)^(-k) = sum_j (c_(j+1) - c_j) (1 + t)^(-j) + c_0 (1 + t)
    std::vector<TermValue> lifted(length);
    for (unsigned int j = 0u; j < length; j += 1u)
    {
        const TermValue next = (j + 1u < length) ? weights[j + 1u] : TermValue();
        lifted[j] = term_value_less(values, next, weights[j]);
    }
    const TermValue fall = term_value_e(values, sim_rational_negative(z));
    const SimRational x = sim_rational_product(two, z);
    const TermValue above = term_value_times(values, fall, term_value_above_sum(values, z, weights));
    // the k = 0 term, int_1^inf e^(-zt) (1 + t) dt = e^(-z) 4 eps_-1(2z)
    TermValue outer = term_value_ratio(values, x, -1);
    term_value_scale(&outer, 4ll, 1ll);
    const TermValue above_slope = term_value_times(
        values, fall, term_value_written(values, term_value_add(values, term_value_above_sum(values, z, lifted), outer)));
    const TermValue inverse = term_value_reciprocal(values, values->gamma_once);
    *value = term_value_written(values, term_value_times(values, term_value_add(values, below, above), inverse));
    *slope = term_value_written(
        values, term_value_negative(term_value_times(values, term_value_add(values, below_slope, above_slope), inverse)));
}

void term_value_begin(TermValues *values)
{
    values->residues.clear();
    values->sums.clear();
    values->runs.clear();
    // residue 0, the integer 1
    term_value_residue(values, 1ull, 0u);
}

void term_value_open(TermValues *values, SimRational h, unsigned int length)
{
    const SimRational one = sim_rational(1ll, 1ll);
    values->h = h;
    values->length = length;
    values->exponentials.clear();
    values->powers.clear();
    values->ratios.clear();
    values->kummers.clear();
    values->e = term_value_exponential(values, one);
    values->gamma_once = term_value_gamma(values, sim_rational_sum(one, h));
    values->gamma_twice = term_value_gamma(values, sim_rational_sum(one, sim_rational_product(sim_rational(2ll, 1ll), h)));
    term_value_integral(values, one, &values->value_at_one, &values->slope_at_one);
    // w's series about 1: a_k = C_k / (k! hd^k), C_(k+2) = hd ((hn + (k+1) hd) C_k - (k+1) C_(k+1)), the first from
    // C_0 = 1, C_1 = 0 and the second from C_0 = 0, C_1 = hd; X = C_(k+1) and Y = C_k step to C_(k+2) and C_(k+1)
    long long top = 0ll;
    long long bottom = 1ll;
    term_value_parts_small(h, &top, &bottom);
    values->first.clear();
    values->second.clear();
    for (unsigned int which = 0u; which < 2u; which += 1u)
    {
        TermValueChain chain = term_value_chain((which == 0u) ? 0ll : bottom, (which == 0u) ? 1ll : 0ll);
        for (unsigned int k = 0u; k + 2u < length; k += 1u)
        {
            const long long at = (long long)k + 1ll;
            term_value_step(&chain, -bottom * at, bottom * (top + at * bottom), 1ll, 0ll);
        }
        unsigned int x_target = 0u;
        const size_t run = values->runs.size();
        term_value_run(values, {chain}, 1, &x_target, NULL);
        const std::vector<unsigned int> history = values->runs[run].chains[0].history;
        std::vector<TermValue> &coefficients = (which == 0u) ? values->first : values->second;
        coefficients.push_back((which == 0u) ? term_value_whole(1ll, 1ll) : TermValue());
        coefficients.push_back((which == 0u) ? TermValue() : term_value_whole(1ll, 1ll));
        TermValuePrimes divisor;
        term_value_factor((unsigned long long)bottom, 1u, &divisor);
        for (unsigned int k = 2u; k < length; k += 1u)
        {
            term_value_factor(k, 1u, &divisor);
            term_value_factor((unsigned long long)bottom, 1u, &divisor);
            TermValue coefficient;
            TermValueRow row;
            row.negative = 0;
            row.residues[history[k - 2u]] = 1u;
            coefficient.rows.push_back(row);
            coefficient.divisor_primes = divisor;
            coefficients.push_back(coefficient);
        }
    }
}

void term_value_kummer(TermValues *values, SimRational z, TermValue *value, TermValue *slope)
{
    const SimRational one = sim_rational(1ll, 1ll);
    if (sim_rational_equal(z, one))
    {
        *value = values->value_at_one;
        *slope = values->slope_at_one;
        return;
    }
    for (const TermValueKummer &kummer : values->kummers)
    {
        if (sim_rational_equal(kummer.z, z))
        {
            *value = kummer.value;
            *slope = kummer.slope;
            return;
        }
    }
    const SimRational offset = sim_rational_difference(z, one);
    if (sim_rational_sign(sim_rational_difference(sim_rational_absolute(offset), one)) >= 0)
    {
        s_term_value_short = 1;
    }
    long long top = 0ll;
    long long bottom = 1ll;
    term_value_parts_small(offset, &top, &bottom);
    const unsigned int length = values->length;
    // value = sum_k a_k d^k w(1) + sum_k b_k d^k w'(1), slope = sum_k k a_k d^(k-1) w(1) + sum_k k b_k d^(k-1) w'(1)
    std::vector<TermValue> sums[4];
    for (unsigned int k = 0u; k < length; k += 1u)
    {
        const TermValue power = term_value_rational_power(top, bottom, k);
        sums[0].push_back(term_value_times(values, values->first[k], power));
        sums[1].push_back(term_value_times(values, values->second[k], power));
        if (k > 0u)
        {
            TermValue lower = term_value_rational_power(top, bottom, k - 1u);
            term_value_scale(&lower, (long long)k, 1ll);
            sums[2].push_back(term_value_times(values, values->first[k], lower));
            sums[3].push_back(term_value_times(values, values->second[k], lower));
        }
    }
    TermValue written[4];
    for (unsigned int which = 0u; which < 4u; which += 1u)
    {
        written[which] = term_value_written(values, term_value_add_all(sums[which]));
    }
    TermValueKummer kummer;
    kummer.z = z;
    kummer.value = term_value_written(values, term_value_add(values, term_value_times(values, written[0], values->value_at_one),
                                                             term_value_times(values, written[1], values->slope_at_one)));
    kummer.slope = term_value_written(values, term_value_add(values, term_value_times(values, written[2], values->value_at_one),
                                                             term_value_times(values, written[3], values->slope_at_one)));
    values->kummers.push_back(kummer);
    *value = kummer.value;
    *slope = kummer.slope;
}

TermValue term_value_linear_tail(TermValues *values, SimRational z_b)
{
    const SimRational h = values->h;
    TermValue value;
    TermValue slope;
    term_value_kummer(values, z_b, &value, &slope);
    const TermValue reach = term_value_times(values, term_value_from(z_b), term_value_power(values, z_b, sim_rational_negative(h)));
    const TermValue square = term_value_from(sim_rational_product(z_b, z_b));
    const TermValue top = term_value_add(values, term_value_times(values, square, term_value_less(values, slope, value)), reach);
    const TermValue lower = term_value_reciprocal(values, term_value_from(sim_rational_difference(sim_rational(1ll, 1ll), h)));
    return term_value_written(values, term_value_times(values, top, lower));
}

// v^shift (1 - v)^power, its coefficients
static std::vector<long long> term_value_polynomial(unsigned int shift, unsigned int power)
{
    std::vector<long long> coefficients(shift + power + 1u, 0ll);
    long long choose = 1ll;
    for (unsigned int index = 0u; index <= power; index += 1u)
    {
        coefficients[shift + index] = choose;
        choose = choose * -((long long)power - (long long)index) / ((long long)index + 1ll);
    }
    return coefficients;
}

// the sum over i of coefficients[i] sums[i], e^(-z) times it
static TermValue term_value_combined(TermValues *values, const std::vector<long long> &coefficients,
                                     const std::vector<TermValue> &sums, const TermValue &fall)
{
    std::vector<TermValue> parts;
    for (size_t i = 0u; i < coefficients.size(); i += 1u)
    {
        if (coefficients[i] != 0ll)
        {
            parts.push_back(term_value_times(values, term_value_whole(coefficients[i], 1ll), sums[i]));
        }
    }
    return term_value_times(values, fall, term_value_written(values, term_value_add_all(parts)));
}

// sum_n D_n G_n^+ and sum_n D_n G_n, each over Gamma(2 + 2h)
static void term_value_square_sums(TermValues *values, SimRational z, TermValue *lifted_sum, TermValue *plain_sum)
{
    const SimRational h = values->h;
    const SimRational one = sim_rational(1ll, 1ll);
    const SimRational two = sim_rational(2ll, 1ll);
    const SimRational twice_h = sim_rational_product(two, h);
    const unsigned int length = values->length;
    long long top = 0ll;
    long long bottom = 1ll;
    term_value_parts_small(h, &top, &bottom);
    // (h)_j (h + 1)_j / j!
    std::vector<TermValue> rising(length, term_value_whole(1ll, 1ll));
    for (unsigned int j = 0u; j + 1u < length; j += 1u)
    {
        rising[j + 1u] = rising[j];
        term_value_scale(&rising[j + 1u], top + (long long)j * bottom, bottom);
        term_value_scale(&rising[j + 1u], top + ((long long)j + 1ll) * bottom, bottom * ((long long)j + 1ll));
    }
    // (1 - v)^(2h - 1)
    const std::vector<TermValue> tail_series = term_value_binomial_series(sim_rational_difference(one, twice_h), length);
    const TermValue half_power = term_value_power(values, two, sim_rational_negative(h));
    const TermValue fall = term_value_e(values, sim_rational_negative(z));
    const SimRational x = sim_rational_product(two, z);
    // G_q = sum_u t_u 2^(1-u-q) eps_(u+q)(2z), q = 0 .. length
    std::vector<TermValue> sums(length + 1u);
    for (unsigned int q = 0u; q <= length; q += 1u)
    {
        std::vector<TermValue> parts;
        for (unsigned int u = 0u; u < length; u += 1u)
        {
            TermValue scale = term_value_rational_power(1ll, 2ll, u + q);
            term_value_scale(&scale, 2ll, 1ll);
            parts.push_back(term_value_times(values, term_value_times(values, tail_series[u], scale),
                                             term_value_ratio(values, x, (int)(u + q))));
        }
        sums[q] = term_value_written(values, term_value_add_all(parts));
    }
    TermValuePrimes plain_divisor;
    TermValuePrimes lifted_divisor;
    for (unsigned int n = 0u; n < length; n += 1u)
    {
        const SimRational a = sim_rational_sum(twice_h, sim_rational((long long)n, 1ll));
        term_value_beta_divisor(values, z, a, a, &plain_divisor);
        term_value_beta_divisor(values, z, a, sim_rational_sum(a, one), &lifted_divisor);
    }
    TermValue scale = term_value_times(values, half_power, half_power);
    TermValue pochhammer = term_value_whole(1ll, 1ll);
    std::vector<TermValue> lifted_parts;
    std::vector<TermValue> plain_parts;
    for (unsigned int n = 0u; n < length; n += 1u)
    {
        std::vector<TermValue> pairs;
        for (unsigned int j = 0u; j <= n; j += 1u)
        {
            pairs.push_back(term_value_times(values, rising[j], rising[n - j]));
        }
        const TermValue weight = term_value_written(
            values, term_value_times(values, term_value_add_all(pairs), term_value_reciprocal(values, pochhammer)));
        const SimRational a = sim_rational_sum(twice_h, sim_rational((long long)n, 1ll));
        const TermValue below = term_value_beta_sum(values, z, a, a, scale, plain_divisor);
        const TermValue below_lifted = term_value_beta_sum(values, z, a, sim_rational_sum(a, one), scale, lifted_divisor);
        const TermValue above = term_value_combined(values, term_value_polynomial(1u, n), sums, fall);
        const TermValue above_lifted = term_value_combined(values, term_value_polynomial(0u, n + 1u), sums, fall);
        plain_parts.push_back(term_value_times(values, weight, term_value_add(values, below, above)));
        lifted_parts.push_back(term_value_times(values, weight, term_value_add(values, below_lifted, above_lifted)));
        pochhammer = term_value_times(values, pochhammer, term_value_whole(2ll * top + (2ll + (long long)n) * bottom, bottom));
        scale = term_value_times(values, scale, term_value_whole(1ll, 2ll));
    }
    const TermValue gamma = term_value_times(values, term_value_from(sim_rational_sum(one, twice_h)), values->gamma_twice);
    const TermValue inverse = term_value_reciprocal(values, gamma);
    *lifted_sum =
        term_value_written(values, term_value_times(values, term_value_written(values, term_value_add_all(lifted_parts)), inverse));
    *plain_sum =
        term_value_written(values, term_value_times(values, term_value_written(values, term_value_add_all(plain_parts)), inverse));
}

TermValue term_value_square_tail(TermValues *values, SimRational z)
{
    const SimRational twice_h = sim_rational_product(sim_rational(2ll, 1ll), values->h);
    TermValue lifted;
    TermValue plain;
    term_value_square_sums(values, z, &lifted, &plain);
    const TermValue reach = term_value_power(values, z, sim_rational_negative(values->h));
    const TermValue top = term_value_add(values, term_value_times(values, term_value_from(z), lifted), plain);
    const TermValue fall =
        term_value_times(values, term_value_times(values, reach, reach), term_value_reciprocal(values, term_value_from(twice_h)));
    return term_value_written(values, term_value_less(values, top, fall));
}

TermValue term_value_square_integral(TermValues *values, SimRational z)
{
    TermValue lifted;
    TermValue plain;
    term_value_square_sums(values, z, &lifted, &plain);
    return lifted;
}

// the text from `start` up to `stop` read as p/q or p: 1, or 0 where it is not one
static int term_value_read(const std::string &text, size_t start, size_t stop, SimRational *value)
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
    *value = sim_rational(numerator, denominator);
    return 1;
}

// the rational between `prefix` at the start of the name and `suffix` at its end
static int term_value_between(const std::string &name, const std::string &prefix, const std::string &suffix,
                              SimRational *value)
{
    if ((name.size() <= prefix.size() + suffix.size()) || (name.compare(0u, prefix.size(), prefix) != 0) ||
        (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0))
    {
        return 0;
    }
    return term_value_read(name, prefix.size(), name.size() - suffix.size(), value);
}

// int_(R)^inf ((T)^(-1-h) x w(x/(T)) - x^(-h)) dx = T^(1-h) int_(R/T)^inf (z w - z^(-h)) dz
static int term_value_heat_tail(TermValues *values, const std::string &name, TermValue *value)
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
    if (!term_value_read(name, open.size(), first, &end) || !term_value_read(spread_text, 0u, spread_text.size(), &spread) ||
        (name.substr(second) != power + spread_text + close))
    {
        return 0;
    }
    const TermValue lift =
        term_value_times(values, term_value_from(spread), term_value_power(values, spread, sim_rational_negative(values->h)));
    const TermValue tail = term_value_linear_tail(values, sim_rational_product(end, sim_rational_reciprocal(spread)));
    *value = term_value_written(values, term_value_times(values, lift, tail));
    return 1;
}

int term_value_named(TermValues *values, const std::string &name, TermValue *value)
{
    const SimRational h = values->h;
    SimRational x;
    if (name == "2^(-1/2)")
    {
        *value = term_value_power(values, sim_rational(2ll, 1ll), sim_rational(-1ll, 2ll));
        return 1;
    }
    if (term_value_between(name, "(", ")^(-h)", &x))
    {
        *value = term_value_power(values, x, sim_rational_negative(h));
        return 1;
    }
    if (term_value_between(name, "w(", ")", &x))
    {
        TermValue slope;
        term_value_kummer(values, x, value, &slope);
        return 1;
    }
    if (term_value_between(name, "w'(", ")", &x))
    {
        TermValue level;
        term_value_kummer(values, x, &level, value);
        return 1;
    }
    if (term_value_between(name, "E1(", ")", &x))
    {
        *value = term_value_written(values, term_value_times(values, term_value_e(values, sim_rational_negative(x)),
                                                             term_value_ratio(values, x, 1)));
        return 1;
    }
    if (term_value_between(name, "rho(", ")", &x))
    {
        const SimRational ratio = sim_rational_difference(sim_rational_reciprocal(x),
                                                          sim_rational_reciprocal(sim_rational_difference(sim_rational(1ll, 1ll), x)));
        *value = term_value_reciprocal(values, term_value_add(values, term_value_whole(1ll, 1ll), term_value_e(values, ratio)));
        return 1;
    }
    if (term_value_between(name, "int_(", ")^inf (z w^2 - z^(-1-2h)) dz", &x))
    {
        *value = term_value_square_tail(values, x);
        return 1;
    }
    if (term_value_between(name, "int_(", ")^inf w^2", &x))
    {
        *value = term_value_square_integral(values, x);
        return 1;
    }
    return term_value_heat_tail(values, name, value);
}

// an exact integer's magnitude as limbs, its top limb not 0
static std::vector<unsigned int> term_value_limbs(const AnchorExactInteger *value)
{
    const unsigned long long used = sim_exact_limbs_used(value);
    return std::vector<unsigned int>(value->limb, value->limb + used);
}

unsigned int term_value_divisor(TermValues *values, const TermValue &value)
{
    if (value.divisor_primes.empty() && value.divisor_residues.empty())
    {
        return 0u;
    }
    if (value.divisor_primes.empty() && (value.divisor_residues.size() == 1u) && (value.divisor_residues.begin()->second == 1u))
    {
        return value.divisor_residues.begin()->first;
    }
    TermValueRow row;
    row.negative = 0;
    row.primes = value.divisor_primes;
    row.residues = value.divisor_residues;
    return term_value_sum(values, {row});
}

int term_value_parts(TermValues *values, const TermValue &value, unsigned int *numerator, unsigned int *divisor)
{
    if (value.rows.empty())
    {
        return 0;
    }
    const TermValue written = term_value_written(values, value);
    *numerator = written.rows[0].residues.begin()->first;
    *divisor = term_value_divisor(values, written);
    return 1;
}

TermValue term_value_form(TermValues *values, const TermForm &form, const std::vector<unsigned int> &numerators,
                          const std::vector<unsigned int> &divisors, const std::vector<int> &zero)
{
    const std::vector<unsigned int> slots = term_form_slots(form);
    // the highest power of each term the form asks for
    std::vector<unsigned int> raised(numerators.size(), 0u);
    for (unsigned int slot : slots)
    {
        const TermKey &key = term_form_key_at(slot);
        for (size_t index = 0u; index < key.power.size(); index += 1u)
        {
            if (index >= numerators.size())
            {
                s_term_value_short = (key.power[index] != 0u) ? 1 : s_term_value_short;
                continue;
            }
            raised[index] = (key.power[index] > raised[index]) ? key.power[index] : raised[index];
        }
    }
    // every slot over D_i^raised_i of each term and the form's denominator: term i enters a slot as N_i^p D_i^(raised_i - p),
    // one residue for each power asked for
    TermValue common;
    for (size_t index = 0u; index < raised.size(); index += 1u)
    {
        if ((raised[index] > 0u) && !zero[index])
        {
            common.divisor_residues[divisors[index]] += raised[index];
        }
    }
    TermValueRow denominator_row;
    denominator_row.negative = 0;
    denominator_row.wide = term_value_limbs(&form.denominator);
    common.divisor_residues[term_value_sum(values, {denominator_row})] += 1u;
    std::map<std::pair<unsigned int, unsigned int>, unsigned int> lifted;
    std::vector<TermValue> parts;
    for (unsigned int slot : slots)
    {
        const TermKey &key = term_form_key_at(slot);
        const AnchorExactInteger &magnitude = form.magnitude[slot];
        int vanishes = magnitude.sign == 0;
        TermValueRow row;
        row.negative = magnitude.sign < 0;
        row.wide = term_value_limbs(&magnitude);
        for (size_t index = 0u; index < raised.size(); index += 1u)
        {
            if (raised[index] == 0u)
            {
                continue;
            }
            const unsigned int power = (index < key.power.size()) ? key.power[index] : 0u;
            if (zero[index])
            {
                vanishes = vanishes || (power > 0u);
                continue;
            }
            const std::pair<unsigned int, unsigned int> asked((unsigned int)index, power);
            if (lifted.find(asked) == lifted.end())
            {
                TermValueRow power_row;
                power_row.negative = 0;
                if (power > 0u)
                {
                    power_row.residues[numerators[index]] += power;
                }
                if (raised[index] > power)
                {
                    power_row.residues[divisors[index]] += raised[index] - power;
                }
                lifted[asked] = term_value_sum(values, {power_row});
            }
            row.residues[lifted[asked]] += 1u;
        }
        if (vanishes)
        {
            continue;
        }
        TermValue part = common;
        part.rows.push_back(row);
        if (sim_rational_sign(key.e) != 0)
        {
            part = term_value_times(values, part, term_value_e(values, key.e));
        }
        parts.push_back(part);
    }
    return term_value_written(values, term_value_add_all(parts));
}

int term_value_short(void)
{
    return s_term_value_short != 0;
}
