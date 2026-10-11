// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// decay_integral.cu: the two-term decay integral (decay_integral.h)
#include "decay_integral.h"

static void decay_integral_trim(std::vector<SimRational> *values)
{
    while (!values->empty() && (sim_rational_sign(values->back()) == 0))
    {
        values->pop_back();
    }
}

void decay_integral_reduction(unsigned int m, std::vector<SimRational> *a, std::vector<SimRational> *b)
{
    a->clear();
    b->assign(1u, sim_rational(1ll, 1ll));
    for (unsigned int order = 1u; order < m; order += 1u)
    {
        const SimRational over = sim_rational(1ll, (long long)order);
        // (1 - x A) / order
        std::vector<SimRational> next_a(a->size() + 1u, sim_rational(0ll, 1ll));
        next_a[0] = over;
        for (size_t index = 0u; index < a->size(); index += 1u)
        {
            next_a[index + 1u] = sim_rational_negative(sim_rational_product((*a)[index], over));
        }
        // -x B / order
        std::vector<SimRational> next_b(b->size() + 1u, sim_rational(0ll, 1ll));
        for (size_t index = 0u; index < b->size(); index += 1u)
        {
            next_b[index + 1u] = sim_rational_negative(sim_rational_product((*b)[index], over));
        }
        decay_integral_trim(&next_a);
        decay_integral_trim(&next_b);
        a->swap(next_a);
        b->swap(next_b);
    }
}

void decay_integral_residual(unsigned int k, std::vector<SimRational> *first, std::vector<SimRational> *second)
{
    std::vector<SimRational> a;
    std::vector<SimRational> b;
    decay_integral_reduction(k + 2u, &a, &b);
    const SimRational rise = sim_rational((long long)k + 1ll, 1ll);
    const size_t size = ((a.size() > b.size()) ? a.size() : b.size()) + 1u;
    first->assign(size, sim_rational(0ll, 1ll));
    second->assign(size, sim_rational(0ll, 1ll));
    // (k + 1) B - x B': the coefficient of x^j is (k + 1 - j) B_j
    for (size_t index = 0u; index < b.size(); index += 1u)
    {
        (*first)[index] = sim_rational_product(sim_rational_difference(rise, sim_rational((long long)index, 1ll)),
                                               b[index]);
    }
    // (k + 1) A - x A' + x A + B - 1
    for (size_t index = 0u; index < a.size(); index += 1u)
    {
        (*second)[index] = sim_rational_sum(
            (*second)[index],
            sim_rational_product(sim_rational_difference(rise, sim_rational((long long)index, 1ll)), a[index]));
        (*second)[index + 1u] = sim_rational_sum((*second)[index + 1u], a[index]);
    }
    for (size_t index = 0u; index < b.size(); index += 1u)
    {
        (*second)[index] = sim_rational_sum((*second)[index], b[index]);
    }
    (*second)[0] = sim_rational_difference((*second)[0], sim_rational(1ll, 1ll));
    decay_integral_trim(first);
    decay_integral_trim(second);
}

static SimRational decay_integral_polynomial(const std::vector<SimRational> &values, SimRational x)
{
    SimRational sum = sim_rational(0ll, 1ll);
    for (size_t index = values.size(); index > 0u; index -= 1u)
    {
        sum = sim_rational_sum(sim_rational_product(sum, x), values[index - 1u]);
    }
    return sum;
}

TermForm decay_integral_from_zero(int k, SimRational n, SimRational b, TermBook *book)
{
    const SimRational x = sim_rational_product(n, sim_rational_reciprocal(b));
    if (k == -1)
    {
        return term_form_term(term_book_id(book, "E1(" + term_book_rational(x) + ")"));
    }
    if (k < -1)
    {
        // E_m = r_m e^(-x), r_0 = 1 / x and r_m = (1 - m r_(m+1)) / x, down to m = k + 2; times b^(k+1)
        const SimRational over = sim_rational_reciprocal(x);
        SimRational share = over;
        for (int m = -1; m >= k + 2; m -= 1)
        {
            share = sim_rational_product(
                sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(sim_rational(m, 1ll), share)),
                over);
        }
        SimRational lift = sim_rational(1ll, 1ll);
        for (int step = k + 1; step < 0; step += 1)
        {
            lift = sim_rational_product(lift, sim_rational_reciprocal(b));
        }
        return term_form_scaled(term_form_e(sim_rational_negative(x)), sim_rational_product(lift, share));
    }
    std::vector<SimRational> a;
    std::vector<SimRational> bb;
    // k is 0 or more here
    decay_integral_reduction((unsigned int)k + 2u, &a, &bb);
    SimRational lift = b;
    for (int step = 0; step < k; step += 1)
    {
        lift = sim_rational_product(lift, b);
    }
    const std::string name = term_book_rational(x);
    const unsigned int integral = term_book_id(book, "E1(" + name + ")");
    const TermForm decay_part = term_form_scaled(term_form_e(sim_rational_negative(x)),
                                                 sim_rational_product(lift, decay_integral_polynomial(a, x)));
    const TermForm integral_part = term_form_scaled(term_form_term(integral),
                                                    sim_rational_product(lift, decay_integral_polynomial(bb, x)));
    return term_form_sum(decay_part, integral_part);
}

SimRational decay_integral_negative_residual(int k, SimRational x)
{
    // r and r' from m = 0 down to k + 2: r_0 = 1/x, r_0' = -1/x^2, r_m = (1 - m r_(m+1)) / x,
    // r_m' = (-m r_(m+1)' x - (1 - m r_(m+1))) / x^2
    const SimRational over = sim_rational_reciprocal(x);
    SimRational share = over;
    SimRational slope = sim_rational_negative(sim_rational_product(over, over));
    for (int m = -1; m >= k + 2; m -= 1)
    {
        const SimRational top = sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(sim_rational(m, 1ll), share));
        const SimRational next_slope = sim_rational_product(
            sim_rational_difference(sim_rational_negative(sim_rational_product(sim_rational_product(sim_rational(m, 1ll), slope), x)),
                                    top),
            sim_rational_product(over, over));
        share = sim_rational_product(top, over);
        slope = next_slope;
    }
    // (k + 1) r - x r' + x r - 1
    return sim_rational_difference(
        sim_rational_sum(sim_rational_difference(sim_rational_product(sim_rational(k + 1, 1ll), share),
                                                 sim_rational_product(x, slope)),
                         sim_rational_product(x, share)),
        sim_rational(1ll, 1ll));
}

int decay_integral_short(void)
{
    return g_sim_rational_wide != 0;
}
