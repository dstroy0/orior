// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_tangent.cu: the axis core's tangent along the pressure datum in the direction e^(mu eta), every order exactly e^(mu
// eta) times a polynomial in mu whose coefficients are exact Chebyshev weights in xi = eta / a
#include "run_cfg.h"

#include "record.h"

#include "core_radius_weights.h"

// The datum enters the core through Z_{-2A} Pi alone, and the tangent of the rule of core_radius_weights.h along a datum
// direction p is the same rule taken once in the tangent of each field: with j = k - i,
//     2 (k+1)(k+2) df_(k+1) = (k + 1 + h) L df_k + D eta L df_k' + 8 h k D eta^2 df_k
//                             + sum (k - i + 1) (dw_i f_j + w_i df_j) + sum (du_i a_j + u_i (Z df)_j),
//     2 (k+1)^2 du_(k+1) = (k + A) L du_k + D eta L du_k' + 8 h k D eta^2 du_k
//                          + sum (k - i) (dw_i u_j + w_i du_j) + sum (du_i b_j + u_i (Z du)_j) + (Z dp)_k,
//     (k+1) dw_k = -(Z du)_k,   (k+1) dp_(k+1) = L^2 sum (df_i f_j + f_i df_j),   dp_0 = p,
// a_j and b_j the base's Z parts. Along p = e^(mu eta), d/deta (e^(mu eta) P) = e^(mu eta) (mu P + P_eta), and each field
// of order k is e^(mu eta) times a polynomial in mu, held as one vector of weights for each power.
//
// Only a path that takes the derivative of e^(mu eta) at every order reaches the power mu^k at order k, and such a path
// reads the base at order 0 alone. The top weights therefore follow, with sigma = D eta + U_0 d,
//     du_1 = L d / 2,   du_(k+1) = sigma L du_k / (2 (k+1)^2),   dw_k = -L d du_k / (k + 1),
//     df_(k+1) = (sigma L df_k + F_0 dw_k) / (2 (k+1)(k+2)),
// each a product of exact weights: the coefficient of mu^k at order k gains a factor near sigma mu / (2 k^2) at every
// order, the loss of one eta-derivative an order, and at a point where sigma and d are past 0 it keeps one sign.
// Checks:
// 1. du_k has degree k in mu from k = 1, and df_k from k = 2.
// 2. The top weights of du_k and df_k are the weights of the products above at every order to the cfg's.
// 3. At the cfg's eta, sigma and d are past 0 and the top coefficient of dF_k = df_k / L^(2k) keeps one sign.
// 4. Every exact value is held in the build's width.
// 5. Every coefficient at the cfg's eta is written whole to the cfg's record.
// The request: core_tangent <cfg>.
//     bash examples/navier_stokes/run.sh core_tangent examples/navier_stokes/cfg/core_tangent.cfg

// one field of one order: entry n the weights of mu^n
typedef std::vector<CoreRadiusWeights> CoreTangentPowers;

static CoreTangentPowers core_tangent_sum(const CoreTangentPowers &left, const CoreTangentPowers &right)
{
    CoreTangentPowers sum = left;
    if (sum.size() < right.size())
    {
        sum.resize(right.size());
    }
    for (size_t n = 0u; n < right.size(); n += 1u)
    {
        sum[n] = core_radius_sum(sum[n], right[n]);
    }
    return sum;
}

static CoreTangentPowers core_tangent_scaled(const CoreTangentPowers &powers, SimRational factor)
{
    CoreTangentPowers scaled;
    for (const CoreRadiusWeights &weights : powers)
    {
        scaled.push_back(core_radius_scaled(weights, factor));
    }
    return scaled;
}

// L, d, eta = a xi and eta^2 taken at each power, exact
static CoreTangentPowers core_tangent_l(const CoreTangentPowers &powers, const CoreRadiusScale *scale)
{
    CoreTangentPowers taken;
    for (const CoreRadiusWeights &weights : powers)
    {
        taken.push_back(core_radius_l(weights, scale, core_radius_number(-1ll, 1ll)));
    }
    return taken;
}

static CoreTangentPowers core_tangent_d(const CoreTangentPowers &powers, const CoreRadiusScale *scale)
{
    CoreTangentPowers taken;
    for (const CoreRadiusWeights &weights : powers)
    {
        taken.push_back(core_radius_d(weights, scale, core_radius_number(-1ll, 1ll)));
    }
    return taken;
}

static CoreTangentPowers core_tangent_eta(const CoreTangentPowers &powers, const CoreRadiusScale *scale)
{
    CoreTangentPowers taken;
    for (const CoreRadiusWeights &weights : powers)
    {
        taken.push_back(core_radius_scaled(core_radius_eta(weights), scale->cut));
    }
    return taken;
}

static CoreTangentPowers core_tangent_eta_square(const CoreTangentPowers &powers, const CoreRadiusScale *scale)
{
    CoreTangentPowers taken;
    for (const CoreRadiusWeights &weights : powers)
    {
        taken.push_back(core_radius_eta_square(weights, scale));
    }
    return taken;
}

// d/deta (e^(mu eta) P) over e^(mu eta): mu P + P_eta, P_eta = (1 / a) P_xi
static CoreTangentPowers core_tangent_slope(const CoreTangentPowers &powers, const CoreRadiusScale *scale)
{
    CoreTangentPowers slope(powers.size() + 1u);
    for (size_t n = 0u; n < powers.size(); n += 1u)
    {
        slope[n] = core_radius_sum(slope[n], core_radius_scaled(core_radius_slope(powers[n]), sim_rational_reciprocal(scale->cut)));
        slope[n + 1u] = core_radius_sum(slope[n + 1u], powers[n]);
    }
    return slope;
}

// the base's weights times each power
static CoreTangentPowers core_tangent_product(const CoreRadiusWeights &base, const CoreTangentPowers &powers)
{
    CoreTangentPowers product;
    for (const CoreRadiusWeights &weights : powers)
    {
        product.push_back(core_radius_product(base, weights));
    }
    return product;
}

// (Z_b g)_j: -(c + 2 j) eta L g + L d g' + 8 h j eta d g, c = -2 b
static CoreTangentPowers core_tangent_turned(const CoreTangentPowers &g, SimRational c, unsigned int j, const CoreRadiusScale *scale)
{
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    const SimRational eta_turn = core_radius_number(-1ll, 1ll);
    CoreTangentPowers turned = core_tangent_scaled(core_tangent_eta(core_tangent_l(g, scale), scale),
                                                   core_radius_times(eta_turn, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j))));
    turned = core_tangent_sum(turned, core_tangent_l(core_tangent_d(core_tangent_slope(g, scale), scale), scale));
    const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
    return core_tangent_sum(turned, core_tangent_scaled(core_tangent_eta(core_tangent_d(g, scale), scale), eight_h_j));
}

// the power of the last nonzero weights, 0 for none
static size_t core_tangent_degree(const CoreTangentPowers &powers)
{
    size_t degree = 0u;
    for (size_t n = 0u; n < powers.size(); n += 1u)
    {
        for (const SimRational &weight : powers[n])
        {
            if (sim_rational_sign(weight) != 0)
            {
                degree = n;
                break;
            }
        }
    }
    return degree;
}

// sum c_m T_m(xi), the three-term rule taken from the top weight down, exact
static SimRational core_tangent_value(const CoreRadiusWeights &weights, SimRational xi)
{
    SimRational after = core_radius_number(0ll, 1ll);
    SimRational later = core_radius_number(0ll, 1ll);
    const SimRational twice = core_radius_times(core_radius_number(2ll, 1ll), xi);
    for (size_t m = weights.size(); m > 1u; m -= 1u)
    {
        const SimRational current = sim_rational_difference(core_radius_plus(weights[m - 1u], core_radius_times(twice, after)), later);
        later = after;
        after = current;
    }
    const SimRational first = weights.empty() ? core_radius_number(0ll, 1ll) : weights[0];
    return sim_rational_difference(core_radius_plus(first, core_radius_times(xi, after)), later);
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    SimRational cut;
    SimRational eta_point;
    std::vector<SimRational> angular;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    unsigned long long order = 0ull;
    const int read = (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) && run_cfg_rational(&cfg, "core.anisotropy", &h) &&
                     run_cfg_rationals(&cfg, "core.axis.angular", &angular) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
                     run_cfg_rationals(&cfg, "core.axis.pressure", &pressure) && run_cfg_rational(&cfg, "ellipse.cut", &cut) &&
                     run_cfg_count(&cfg, "tangent.order", &order) && (order >= 2ull) && run_cfg_rational(&cfg, "tangent.eta", &eta_point);
    if (!read)
    {
        run_cfg_missing(&results.line, "core anisotropy, axis angular, axial and pressure, an ellipse cut, a tangent order at least 2 and a tangent eta");
        sim_flush(&results);
        fprintf(stderr, "core_tangent <cfg>\n");
        return 2;
    }
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational half = core_radius_number(1ll, 2ll);
    const SimRational minus = core_radius_number(-1ll, 1ll);
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    CoreRadiusScale scale;
    scale.h = h;
    scale.cut = cut;
    scale.d = sim_rational_difference(half, h);
    scale.a = core_radius_plus(half, h);
    const SimRational xi = core_radius_over(eta_point, cut);
    const int legal = (sim_rational_sign(h) >= 0) && (sim_rational_sign(sim_rational_difference(half, h)) >= 0) && (sim_rational_sign(cut) > 0) &&
                      (sim_rational_sign(sim_rational_difference(one, cut)) >= 0) && (sim_rational_sign(core_radius_plus(xi, one)) > 0) &&
                      (sim_rational_sign(sim_rational_difference(one, xi)) > 0);
    sim_check(&results, legal, "the anisotropy in [0, 1/2], the cut in (0, 1], and the eta inside the cut");
    const unsigned int last = (unsigned int)order;
    // the base, exact, to the tangent's order
    CoreRadiusOrders exact;
    core_radius_weights(&scale, angular, axial, pressure, last, minus, &exact);
    const std::vector<CoreRadiusWeights> &f = exact.f;
    const std::vector<CoreRadiusWeights> &u = exact.g;
    const std::vector<CoreRadiusWeights> &w = exact.inflow;
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, h));
    const SimRational along_c = core_radius_times(two, scale.a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale.a);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), h);
    std::vector<CoreRadiusWeights> angular_turned;
    std::vector<CoreRadiusWeights> along_turned;
    for (unsigned int k = 0u; k <= last; k += 1u)
    {
        angular_turned.push_back(core_radius_turned_weights(f[k], angular_c, k, &scale, minus));
        along_turned.push_back(core_radius_turned_weights(u[k], along_c, k, &scale, minus));
    }
    // the tangent along e^(mu eta): df_0 = du_0 = 0, dp_0 = 1 at mu^0
    std::vector<CoreTangentPowers> df = {CoreTangentPowers()};
    std::vector<CoreTangentPowers> du = {CoreTangentPowers()};
    std::vector<CoreTangentPowers> dp = {CoreTangentPowers{CoreRadiusWeights{one}}};
    std::vector<CoreTangentPowers> dw;
    std::vector<CoreTangentPowers> df_turned;
    std::vector<CoreTangentPowers> du_turned;
    for (unsigned int k = 0u; k < last; k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        du_turned.push_back(core_tangent_turned(du[k], along_c, k, &scale));
        df_turned.push_back(core_tangent_turned(df[k], angular_c, k, &scale));
        dw.push_back(core_tangent_scaled(du_turned[k], core_radius_times(minus, sim_rational_reciprocal(next))));
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale.d);
        CoreTangentPowers spin = core_tangent_scaled(core_tangent_l(df[k], &scale), core_radius_plus(next, h));
        spin = core_tangent_sum(spin, core_tangent_scaled(core_tangent_eta(core_tangent_l(core_tangent_slope(df[k], &scale), &scale), &scale), scale.d));
        spin = core_tangent_sum(spin, core_tangent_scaled(core_tangent_eta_square(df[k], &scale), d_eight));
        CoreTangentPowers along = core_tangent_scaled(core_tangent_l(du[k], &scale), core_radius_plus(order_k, scale.a));
        along = core_tangent_sum(along, core_tangent_scaled(core_tangent_eta(core_tangent_l(core_tangent_slope(du[k], &scale), &scale), &scale), scale.d));
        along = core_tangent_sum(along, core_tangent_scaled(core_tangent_eta_square(du[k], &scale), d_eight));
        along = core_tangent_sum(along, core_tangent_turned(dp[k], pressure_c, k, &scale));
        CoreTangentPowers square;
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            const SimRational rest = core_radius_number((long long)j, 1ll);
            spin = core_tangent_sum(spin, core_tangent_scaled(core_tangent_sum(core_tangent_product(f[j], dw[i]), core_tangent_product(w[i], df[j])), core_radius_plus(rest, one)));
            spin = core_tangent_sum(spin, core_tangent_sum(core_tangent_product(angular_turned[j], du[i]), core_tangent_product(u[i], df_turned[j])));
            along = core_tangent_sum(along, core_tangent_scaled(core_tangent_sum(core_tangent_product(u[j], dw[i]), core_tangent_product(w[i], du[j])), rest));
            along = core_tangent_sum(along, core_tangent_sum(core_tangent_product(along_turned[j], du[i]), core_tangent_product(u[i], du_turned[j])));
            square = core_tangent_sum(square, core_tangent_scaled(core_tangent_product(f[i], df[j]), two));
        }
        df.push_back(core_tangent_scaled(spin, sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one)))));
        du.push_back(core_tangent_scaled(along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next)))));
        dp.push_back(core_tangent_scaled(core_tangent_l(core_tangent_l(square, &scale), &scale), sim_rational_reciprocal(next)));
    }
    // 1 and 2: the degrees, and the top weights against the products of sigma L
    const CoreRadiusWeights f0 = f[0];
    const CoreRadiusWeights u0 = u[0];
    CoreRadiusWeights top_u = core_radius_scaled(core_radius_l(core_radius_d(CoreRadiusWeights{one}, &scale, minus), &scale, minus), half);
    CoreRadiusWeights top_f;
    int degrees = 1;
    int tops = 1;
    for (unsigned int k = 1u; k <= last; k += 1u)
    {
        degrees = degrees && (core_tangent_degree(du[k]) == k) && ((k < 2u) || (core_tangent_degree(df[k]) == k));
        tops = tops && (du[k].size() > k) && core_radius_same(du[k][k], top_u) && ((k < 2u) || ((df[k].size() > k) && core_radius_same(df[k][k], top_f)));
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        // sigma L g = D eta L g + U_0 L d g
        const CoreRadiusWeights l_u = core_radius_l(top_u, &scale, minus);
        const CoreRadiusWeights sigma_u = core_radius_sum(core_radius_scaled(core_radius_scaled(core_radius_eta(l_u), cut), scale.d),
                                                          core_radius_product(u0, core_radius_l(core_radius_d(top_u, &scale, minus), &scale, minus)));
        const CoreRadiusWeights l_f = core_radius_l(top_f, &scale, minus);
        const CoreRadiusWeights sigma_f = core_radius_sum(core_radius_scaled(core_radius_scaled(core_radius_eta(l_f), cut), scale.d),
                                                          core_radius_product(u0, core_radius_l(core_radius_d(top_f, &scale, minus), &scale, minus)));
        const CoreRadiusWeights top_w = core_radius_scaled(core_radius_l(core_radius_d(top_u, &scale, minus), &scale, minus), core_radius_times(minus, sim_rational_reciprocal(next)));
        top_f = core_radius_scaled(core_radius_sum(sigma_f, core_radius_product(f0, top_w)),
                                   sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))));
        top_u = core_radius_scaled(sigma_u, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next))));
    }
    sim_check(&results, degrees, "du_k has degree k in mu from k = 1 and df_k from k = 2");
    sim_check(&results, tops, "the top weights of du_k and df_k are the products of sigma L at every order");
    // 3: at the cfg's eta, sigma, d and the sign of the top coefficient of dF_k = df_k / L^(2k)
    const SimRational eta_square = core_radius_times(eta_point, eta_point);
    const SimRational l_point = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_square));
    const SimRational d_point = sim_rational_difference(one, eta_square);
    const SimRational u0_point = core_tangent_value(u0, xi);
    const SimRational sigma_point = core_radius_plus(core_radius_times(scale.d, eta_point), core_radius_times(u0_point, d_point));
    int sign = 0;
    int one_sign = (sim_rational_sign(sigma_point) > 0) && (sim_rational_sign(d_point) > 0) && (sim_rational_sign(l_point) > 0);
    if (record != NULL)
    {
        record_text(record, ("tangent along e^(mu eta) at eta " + term_book_rational(eta_point) + ", xi " + term_book_rational(xi) + ", sigma " +
                             term_book_rational(sigma_point) + ": for each k, the coefficients of mu^0 to mu^k of dF_k e^(-mu eta)")
                                .c_str());
    }
    SimRational l_power = one;
    for (unsigned int k = 1u; k <= last; k += 1u)
    {
        l_power = core_radius_times(l_power, core_radius_times(l_point, l_point));
        std::string row = "  " + std::to_string(k);
        for (size_t n = 0u; n < df[k].size(); n += 1u)
        {
            row += " " + term_book_rational(core_radius_over(core_tangent_value(df[k][n], xi), l_power));
        }
        if (record != NULL)
        {
            record_text(record, row.c_str());
        }
        if (k >= 2u)
        {
            const int top_sign = sim_rational_sign(core_tangent_value(df[k][k], xi));
            one_sign = one_sign && (top_sign != 0) && ((sign == 0) || (top_sign == sign));
            sign = top_sign;
        }
    }
    sim_check(&results, one_sign, "sigma, d and L past 0 at the eta, and the top coefficient of dF_k of one sign at every order");
    scriptura_text(&results.line, "  sigma at the eta: ");
    sim_rational_print(&results.line, sigma_point);
    scriptura_text(&results.line, (sign < 0) ? ", the top coefficients below 0\n" : ", the top coefficients past 0\n");
    sim_flush(&results);
    const int held = !run_cfg_short() && !record_short() && (s_sim_rational_wide == 0);
    scriptura_text(&results.line, held ? "  every value is exact and held in the build's width\n" : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every value held");
    const int recorded = (record != NULL) && record_close(record);
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core tangent");
}
