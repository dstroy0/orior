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
// 6. Where the cfg holds `apart`, the tangent with every part off is the principal system at the vertex, order by order.
// 7. Where the cfg holds `phase`, the root squared is s / s_0 and the root times the quotient is s_eta, at every order.
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

// The measurement at the vertex eta_v = a (rho + 1 / rho) / 2 of E_rho. With mu fixed, each order of the tangent is one
// vector of weights, d/deta (e^(mu eta) P) over e^(mu eta) = mu P + P_eta, and it is read at the vertex. The principal
// tangent, the paths that take the derivative of e^(mu eta) at every step, is a system at the point alone:
//     2 (k+1)^2 y_(k+1) = mu (D eta L y_k + L d sum u_i y_(k-i)) + mu L d [k = 0] + sum (k - i) w_i u_(k-i),
//     (k+1) w_k = -mu L d y_k,   2 (k+1)(k+2) g_(k+1) = mu (D eta L g_k + L d sum u_i g_(k-i)) + sum (k - i + 1) w_i f_(k-i),
// u_i and f_i the base's numerators at the point. Its growth along Y = X / L^2 is e^(sqrt(mu) Phi(Y)), Phi the integral
// of sqrt(s / (2 Y)), s = L (D eta + d U(Y)) = sum s_k Y^k: with sqrt(s / s_0) = sum e_k Y^k,
//     Phi(Y) = sqrt(2 s_0 Y) sum e_k Y^k / (2 k + 1),
// every e_k exact. The record holds each value whole.

// The parts of the full tangent the principal system leaves out: the slope, P_eta in d/deta (e^(mu eta) P); the plain
// terms, every term that takes no derivative; and the pressure, dp_k past k = 0. With every part off the full tangent is
// the principal one, and with every part on it is the full one.
typedef struct
{
    int slope;
    int plain;
    int pressure;
} CoreTangentParts;

// mu P + P_eta for mu fixed, P_eta where the slope is on
static CoreRadiusWeights core_tangent_slope_at(const CoreRadiusWeights &weights, SimRational mu, const CoreRadiusScale *scale, const CoreTangentParts *parts)
{
    const CoreRadiusWeights carried = core_radius_scaled(weights, mu);
    return parts->slope ? core_radius_sum(carried, core_radius_scaled(core_radius_slope(weights), sim_rational_reciprocal(scale->cut))) : carried;
}

// (Z_b g)_j for mu fixed: L d (mu g + g_eta), and the plain -(c + 2 j) eta L g + 8 h j eta d g where the plain terms are on
static CoreRadiusWeights core_tangent_turned_at(const CoreRadiusWeights &g, SimRational c, unsigned int j, SimRational mu, const CoreRadiusScale *scale,
                                                const CoreTangentParts *parts)
{
    const SimRational minus = core_radius_number(-1ll, 1ll);
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    CoreRadiusWeights turned = core_radius_l(core_radius_d(core_tangent_slope_at(g, mu, scale, parts), scale, minus), scale, minus);
    if (parts->plain)
    {
        const SimRational eta_turn = core_radius_times(minus, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j)));
        turned = core_radius_sum(turned, core_radius_scaled(core_radius_eta(core_radius_l(g, scale, minus)), core_radius_times(eta_turn, scale->cut)));
        const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
        turned = core_radius_sum(turned, core_radius_scaled(core_radius_eta(core_radius_d(g, scale, minus)), core_radius_times(eight_h_j, scale->cut)));
    }
    return turned;
}

// the tangent's axial and swirl numerators at the point, order by order to `last`, for mu fixed, with the parts given
static void core_tangent_full_at(const CoreRadiusOrders *exact, unsigned int last, SimRational mu, const CoreRadiusScale *scale, SimRational xi,
                                 const CoreTangentParts *parts, CoreRadiusSequence *axial_at, CoreRadiusSequence *swirl_at)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational minus = core_radius_number(-1ll, 1ll);
    const std::vector<CoreRadiusWeights> &f = exact->f;
    const std::vector<CoreRadiusWeights> &u = exact->g;
    const std::vector<CoreRadiusWeights> &w = exact->inflow;
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_times(two, scale->a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale->a);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), scale->h);
    std::vector<CoreRadiusWeights> angular_turned;
    std::vector<CoreRadiusWeights> along_turned;
    for (unsigned int k = 0u; k <= last; k += 1u)
    {
        angular_turned.push_back(core_radius_turned_weights(f[k], angular_c, k, scale, minus));
        along_turned.push_back(core_radius_turned_weights(u[k], along_c, k, scale, minus));
    }
    std::vector<CoreRadiusWeights> df = {CoreRadiusWeights()};
    std::vector<CoreRadiusWeights> du = {CoreRadiusWeights()};
    std::vector<CoreRadiusWeights> dp = {CoreRadiusWeights{one}};
    std::vector<CoreRadiusWeights> dw;
    std::vector<CoreRadiusWeights> df_turned;
    std::vector<CoreRadiusWeights> du_turned;
    for (unsigned int k = 0u; k < last; k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        du_turned.push_back(core_tangent_turned_at(du[k], along_c, k, mu, scale, parts));
        df_turned.push_back(core_tangent_turned_at(df[k], angular_c, k, mu, scale, parts));
        dw.push_back(core_radius_scaled(du_turned[k], core_radius_times(minus, sim_rational_reciprocal(next))));
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale->d);
        const SimRational eta_d = core_radius_times(scale->cut, scale->d);
        CoreRadiusWeights spin = core_radius_scaled(core_radius_eta(core_radius_l(core_tangent_slope_at(df[k], mu, scale, parts), scale, minus)), eta_d);
        CoreRadiusWeights along = core_radius_scaled(core_radius_eta(core_radius_l(core_tangent_slope_at(du[k], mu, scale, parts), scale, minus)), eta_d);
        if (parts->plain)
        {
            spin = core_radius_sum(spin, core_radius_scaled(core_radius_l(df[k], scale, minus), core_radius_plus(next, scale->h)));
            spin = core_radius_sum(spin, core_radius_scaled(core_radius_eta_square(df[k], scale), d_eight));
            along = core_radius_sum(along, core_radius_scaled(core_radius_l(du[k], scale, minus), core_radius_plus(order_k, scale->a)));
            along = core_radius_sum(along, core_radius_scaled(core_radius_eta_square(du[k], scale), d_eight));
        }
        if ((k == 0u) || parts->pressure)
        {
            along = core_radius_sum(along, core_tangent_turned_at(dp[k], pressure_c, k, mu, scale, parts));
        }
        CoreRadiusWeights square;
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            const SimRational rest = core_radius_number((long long)j, 1ll);
            spin = core_radius_sum(spin, core_radius_scaled(core_radius_product(f[j], dw[i]), core_radius_plus(rest, one)));
            spin = core_radius_sum(spin, core_radius_product(u[i], df_turned[j]));
            along = core_radius_sum(along, core_radius_scaled(core_radius_product(u[j], dw[i]), rest));
            along = core_radius_sum(along, core_radius_product(u[i], du_turned[j]));
            if (parts->plain)
            {
                spin = core_radius_sum(spin, core_radius_scaled(core_radius_product(w[i], df[j]), core_radius_plus(rest, one)));
                spin = core_radius_sum(spin, core_radius_product(angular_turned[j], du[i]));
                along = core_radius_sum(along, core_radius_scaled(core_radius_product(w[i], du[j]), rest));
                along = core_radius_sum(along, core_radius_product(along_turned[j], du[i]));
            }
            square = core_radius_sum(square, core_radius_scaled(core_radius_product(f[i], df[j]), two));
        }
        df.push_back(core_radius_scaled(spin, sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one)))));
        du.push_back(core_radius_scaled(along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next)))));
        dp.push_back(core_radius_scaled(core_radius_l(core_radius_l(square, scale, minus), scale, minus), sim_rational_reciprocal(next)));
    }
    axial_at->clear();
    swirl_at->clear();
    for (unsigned int k = 0u; k <= last; k += 1u)
    {
        axial_at->push_back(core_radius_value(du[k], xi));
        swirl_at->push_back(core_radius_value(df[k], xi));
    }
}

// the principal tangent at the point for mu fixed: the axial y_k and the swirl g_k to `last`
static void core_tangent_principal_at(const CoreRadiusSequence &u, const CoreRadiusSequence &f, SimRational l_point, SimRational d_point,
                                      SimRational eta_point, SimRational dd, unsigned int last, SimRational mu, CoreRadiusSequence *axial_at,
                                      CoreRadiusSequence *swirl_at)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational ld = core_radius_times(l_point, d_point);
    const SimRational transport = core_radius_times(core_radius_times(dd, eta_point), l_point);
    CoreRadiusSequence y = {core_radius_number(0ll, 1ll)};
    CoreRadiusSequence g = {core_radius_number(0ll, 1ll)};
    CoreRadiusSequence w;
    for (unsigned int k = 0u; k < last; k += 1u)
    {
        const SimRational next = core_radius_number((long long)k + 1ll, 1ll);
        w.push_back(core_radius_over(core_radius_times(core_radius_times(core_radius_number(-1ll, 1ll), core_radius_times(mu, ld)), y[k]), next));
        SimRational along = core_radius_times(transport, y[k]);
        SimRational spin = core_radius_times(transport, g[k]);
        SimRational along_sum = core_radius_number(0ll, 1ll);
        SimRational spin_sum = core_radius_number(0ll, 1ll);
        SimRational inflow_along = core_radius_number(0ll, 1ll);
        SimRational inflow_spin = core_radius_number(0ll, 1ll);
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            along_sum = core_radius_plus(along_sum, core_radius_times(u[i], y[j]));
            spin_sum = core_radius_plus(spin_sum, core_radius_times(u[i], g[j]));
            inflow_along = core_radius_plus(inflow_along, core_radius_times(core_radius_times(core_radius_number((long long)j, 1ll), w[i]), u[j]));
            inflow_spin = core_radius_plus(inflow_spin, core_radius_times(core_radius_times(core_radius_number((long long)j + 1ll, 1ll), w[i]), f[j]));
        }
        along = core_radius_times(mu, core_radius_plus(along, core_radius_times(ld, along_sum)));
        spin = core_radius_times(mu, core_radius_plus(spin, core_radius_times(ld, spin_sum)));
        if (k == 0u)
        {
            along = core_radius_plus(along, core_radius_times(mu, ld));
        }
        along = core_radius_plus(along, inflow_along);
        spin = core_radius_plus(spin, inflow_spin);
        y.push_back(core_radius_over(along, core_radius_times(two, core_radius_times(next, next))));
        g.push_back(core_radius_over(spin, core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))));
    }
    *axial_at = y;
    *swirl_at = g;
}

// sum c_k t^k
static SimRational core_tangent_sum_at(const CoreRadiusSequence &c, SimRational t)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    for (size_t k = c.size(); k > 0u; k -= 1u)
    {
        sum = core_radius_plus(core_radius_times(sum, t), c[k - 1u]);
    }
    return sum;
}

// sqrt(series / series_0) = sum e_n Y^n, e_0 = 1: 2 e_n = series_n / series_0 - sum_(i=1..n-1) e_i e_(n-i)
static CoreRadiusSequence core_tangent_root(const CoreRadiusSequence &series, unsigned int reach)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusSequence root = {core_radius_number(1ll, 1ll)};
    for (unsigned int power = 1u; power <= reach; power += 1u)
    {
        SimRational inner = core_radius_over(series[power], series[0]);
        for (unsigned int first = 1u; first < power; first += 1u)
        {
            inner = sim_rational_difference(inner, core_radius_times(root[first], root[power - first]));
        }
        root.push_back(core_radius_times(half, inner));
    }
    return root;
}

// top / bottom as a series, bottom_0 = 1: q_n = top_n - sum_(i=1..n) bottom_i q_(n-i)
static CoreRadiusSequence core_tangent_quotient(const CoreRadiusSequence &top, const CoreRadiusSequence &bottom, unsigned int reach)
{
    CoreRadiusSequence quotient;
    for (unsigned int power = 0u; power <= reach; power += 1u)
    {
        SimRational inner = top[power];
        for (unsigned int first = 1u; first <= power; first += 1u)
        {
            inner = sim_rational_difference(inner, core_radius_times(bottom[first], quotient[power - first]));
        }
        quotient.push_back(inner);
    }
    return quotient;
}

// the coefficient of Y^power in (sum left_n Y^n) (sum right_n Y^n)
static SimRational core_tangent_cauchy(const CoreRadiusSequence &left, const CoreRadiusSequence &right, unsigned int power)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    for (unsigned int first = 0u; first <= power; first += 1u)
    {
        sum = core_radius_plus(sum, core_radius_times(left[first], right[power - first]));
    }
    return sum;
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
    const SimRational u0_point = core_radius_value(u0, xi);
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
            row += " " + term_book_rational(core_radius_over(core_radius_value(df[k][n], xi), l_power));
        }
        if (record != NULL)
        {
            record_text(record, row.c_str());
        }
        if (k >= 2u)
        {
            const int top_sign = sim_rational_sign(core_radius_value(df[k][k], xi));
            one_sign = one_sign && (top_sign != 0) && ((sign == 0) || (top_sign == sign));
            sign = top_sign;
        }
    }
    sim_check(&results, one_sign, "sigma, d and L past 0 at the eta, and the top coefficient of dF_k of one sign at every order");
    scriptura_text(&results.line, "  sigma at the eta: ");
    sim_rational_print(&results.line, sigma_point);
    scriptura_text(&results.line, (sign < 0) ? ", the top coefficients below 0\n" : ", the top coefficients past 0\n");
    sim_flush(&results);
    // the measurement at the vertex, where the cfg holds it
    SimRational measure_rho;
    SimRational measure_step;
    unsigned long long measure_order = 0ull;
    unsigned long long measure_count = 0ull;
    std::vector<SimRational> measure_mu;
    const int measure = run_cfg_rational(&cfg, "measure.rho", &measure_rho) && (sim_rational_sign(sim_rational_difference(measure_rho, one)) > 0) &&
                        run_cfg_count(&cfg, "measure.order", &measure_order) && (measure_order >= 2ull) && run_cfg_rationals(&cfg, "measure.mu", &measure_mu) &&
                        run_cfg_rational(&cfg, "measure.step", &measure_step) && run_cfg_count(&cfg, "measure.count", &measure_count);
    if (measure)
    {
        const unsigned int reach = (unsigned int)measure_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, angular, axial, pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(measure_rho, sim_rational_reciprocal(measure_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v;
        CoreRadiusSequence f_v;
        for (unsigned int k = 0u; k <= reach; k += 1u)
        {
            u_v.push_back(core_radius_value(base.g[k], xi_v));
            f_v.push_back(core_radius_value(base.f[k], xi_v));
        }
        // s = L (D eta + d U), and sqrt(s / s_0) = sum e_k Y^k
        CoreRadiusSequence transport;
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            const SimRational inner = core_radius_times(d_v, u_v[power]);
            transport.push_back(core_radius_times(l_v, (power == 0u) ? core_radius_plus(core_radius_times(scale.d, eta_v), inner) : inner));
        }
        const CoreRadiusSequence root = core_tangent_root(transport, reach);
        CoreRadiusSequence phi_series;
        for (unsigned int k = 0u; k <= reach; k += 1u)
        {
            phi_series.push_back(core_radius_over(root[k], core_radius_number(2ll * (long long)k + 1ll, 1ll)));
        }
        if (record != NULL)
        {
            record_text(record, ("measurement at the vertex eta " + term_book_rational(eta_v) + " of E_rho, rho " + term_book_rational(measure_rho) + ", to order " +
                                 std::to_string(reach) + ": s_0 " + term_book_rational(transport[0]) +
                                 "; then Y and phi, Phi = sqrt(2 s_0 Y) phi; then for each mu, Y, principal axial, full axial, principal swirl, full swirl")
                                    .c_str());
            for (unsigned long long index = 1ull; index <= measure_count; index += 1ull)
            {
                const SimRational y_point = core_radius_times(measure_step, core_radius_number((long long)index, 1ll));
                record_text(record, ("  " + term_book_rational(y_point) + " " + term_book_rational(core_tangent_sum_at(phi_series, y_point))).c_str());
            }
        }
        for (const SimRational &mu : measure_mu)
        {
            CoreRadiusSequence axial_full;
            CoreRadiusSequence swirl_full;
            const CoreTangentParts every = {1, 1, 1};
            core_tangent_full_at(&base, reach, mu, &scale, xi_v, &every, &axial_full, &swirl_full);
            CoreRadiusSequence axial_principal;
            CoreRadiusSequence swirl_principal;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            if (record != NULL)
            {
                record_text(record, ("mu " + term_book_rational(mu)).c_str());
                for (unsigned long long index = 1ull; index <= measure_count; index += 1ull)
                {
                    const SimRational y_point = core_radius_times(measure_step, core_radius_number((long long)index, 1ll));
                    record_text(record, ("  " + term_book_rational(y_point) + " " + term_book_rational(core_tangent_sum_at(axial_principal, y_point)) + " " +
                                         term_book_rational(core_tangent_sum_at(axial_full, y_point)) + " " +
                                         term_book_rational(core_tangent_sum_at(swirl_principal, y_point)) + " " +
                                         term_book_rational(core_tangent_sum_at(swirl_full, y_point)))
                                            .c_str());
                }
            }
        }
        scriptura_text(&results.line, "  measurement at the vertex written to the record\n");
        sim_flush(&results);
    }
    // the remainder taken apart at the vertex, where the cfg holds it: each part the principal system leaves out, alone
    SimRational apart_rho;
    SimRational apart_step;
    unsigned long long apart_order = 0ull;
    unsigned long long apart_count = 0ull;
    std::vector<SimRational> apart_mu;
    const int apart = run_cfg_rational(&cfg, "apart.rho", &apart_rho) && (sim_rational_sign(sim_rational_difference(apart_rho, one)) > 0) &&
                      run_cfg_count(&cfg, "apart.order", &apart_order) && (apart_order >= 2ull) && run_cfg_rationals(&cfg, "apart.mu", &apart_mu) &&
                      run_cfg_rational(&cfg, "apart.step", &apart_step) && run_cfg_count(&cfg, "apart.count", &apart_count);
    if (apart)
    {
        const unsigned int reach = (unsigned int)apart_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, angular, axial, pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(apart_rho, sim_rational_reciprocal(apart_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v;
        CoreRadiusSequence f_v;
        for (unsigned int k = 0u; k <= reach; k += 1u)
        {
            u_v.push_back(core_radius_value(base.g[k], xi_v));
            f_v.push_back(core_radius_value(base.f[k], xi_v));
        }
        // none, the slope alone, the plain terms alone, the pressure alone, and every part
        const CoreTangentParts choices[5] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
        int principal_same = 1;
        if (record != NULL)
        {
            record_text(record, ("remainder taken apart at the vertex eta " + term_book_rational(eta_v) + " of E_rho, rho " + term_book_rational(apart_rho) +
                                 ", to order " + std::to_string(reach) +
                                 ": for each mu, Y, then axial and swirl each for the principal, the slope alone, the plain terms alone, the pressure alone, and every part")
                                    .c_str());
        }
        for (const SimRational &mu : apart_mu)
        {
            CoreRadiusSequence axial_parts[5];
            CoreRadiusSequence swirl_parts[5];
            for (unsigned int choice = 0u; choice < 5u; choice += 1u)
            {
                core_tangent_full_at(&base, reach, mu, &scale, xi_v, &choices[choice], &axial_parts[choice], &swirl_parts[choice]);
            }
            CoreRadiusSequence axial_principal;
            CoreRadiusSequence swirl_principal;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            for (unsigned int k = 0u; k <= reach; k += 1u)
            {
                principal_same = principal_same && (sim_rational_sign(sim_rational_difference(axial_parts[0][k], axial_principal[k])) == 0) &&
                                 (sim_rational_sign(sim_rational_difference(swirl_parts[0][k], swirl_principal[k])) == 0);
            }
            if (record != NULL)
            {
                record_text(record, ("mu " + term_book_rational(mu)).c_str());
                for (unsigned long long index = 1ull; index <= apart_count; index += 1ull)
                {
                    const SimRational y_point = core_radius_times(apart_step, core_radius_number((long long)index, 1ll));
                    std::string row = "  " + term_book_rational(y_point);
                    for (unsigned int choice = 0u; choice < 5u; choice += 1u)
                    {
                        row += " " + term_book_rational(core_tangent_sum_at(axial_parts[choice], y_point));
                    }
                    for (unsigned int choice = 0u; choice < 5u; choice += 1u)
                    {
                        row += " " + term_book_rational(core_tangent_sum_at(swirl_parts[choice], y_point));
                    }
                    record_text(record, row.c_str());
                }
            }
        }
        sim_check(&results, principal_same, "with every part off the tangent's weights read at the vertex are the principal system's, order by order");
    }
    // the slope's share at larger mu, where the cfg holds it: the principal system at the point and the tangent with the
    // slope alone, each with the size of its last order's term over its sum, which says how far the order resolves it
    SimRational slope_rho;
    unsigned long long slope_order = 0ull;
    std::vector<SimRational> slope_mu;
    std::vector<SimRational> slope_y;
    const int slope_share = run_cfg_rational(&cfg, "slope.rho", &slope_rho) && (sim_rational_sign(sim_rational_difference(slope_rho, one)) > 0) &&
                            run_cfg_count(&cfg, "slope.order", &slope_order) && (slope_order >= 2ull) && run_cfg_rationals(&cfg, "slope.mu", &slope_mu) &&
                            run_cfg_rationals(&cfg, "slope.y", &slope_y);
    if (slope_share)
    {
        const unsigned int reach = (unsigned int)slope_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, angular, axial, pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(slope_rho, sim_rational_reciprocal(slope_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v;
        CoreRadiusSequence f_v;
        for (unsigned int k = 0u; k <= reach; k += 1u)
        {
            u_v.push_back(core_radius_value(base.g[k], xi_v));
            f_v.push_back(core_radius_value(base.f[k], xi_v));
        }
        if (record != NULL)
        {
            record_text(record, ("slope's share at the vertex eta " + term_book_rational(eta_v) + " of E_rho, rho " + term_book_rational(slope_rho) + ", to order " +
                                 std::to_string(reach) +
                                 ": for each mu, Y, then axial and swirl each as principal, slope alone, and the last order's term over the sum for each")
                                    .c_str());
        }
        const CoreTangentParts slope_alone = {1, 0, 0};
        for (const SimRational &mu : slope_mu)
        {
            CoreRadiusSequence axial_slope;
            CoreRadiusSequence swirl_slope;
            core_tangent_full_at(&base, reach, mu, &scale, xi_v, &slope_alone, &axial_slope, &swirl_slope);
            CoreRadiusSequence axial_principal;
            CoreRadiusSequence swirl_principal;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            if (record != NULL)
            {
                record_text(record, ("mu " + term_book_rational(mu)).c_str());
                for (const SimRational &y_point : slope_y)
                {
                    SimRational last_power = one;
                    for (unsigned int k = 0u; k < reach; k += 1u)
                    {
                        last_power = core_radius_times(last_power, y_point);
                    }
                    std::string row = "  " + term_book_rational(y_point);
                    const CoreRadiusSequence *const series[4] = {&axial_principal, &axial_slope, &swirl_principal, &swirl_slope};
                    for (unsigned int which = 0u; which < 4u; which += 1u)
                    {
                        const SimRational sum = core_tangent_sum_at(*series[which], y_point);
                        const SimRational last_term = core_radius_times((*series[which])[reach], last_power);
                        row += " " + term_book_rational(sum) + " " + term_book_rational(core_radius_over(last_term, sum));
                    }
                    record_text(record, row.c_str());
                }
            }
        }
    }
    // the phase in both directions at the vertex, where the cfg holds it. The slope turns mu into mu + d/deta at Y fixed, and
    // with the growth e^(sqrt(mu) Psi(Y, eta)) the principal system's 2 Y Psi_Y^2 = s becomes 2 Y mu Psi_Y^2 = s (mu + sqrt(mu) Psi_eta):
    // Psi = Phi + Phi_1 / sqrt(mu) + ..., Phi_1 = (1/2) int_0^Y Phi' Phi_eta dY. With sqrt(s) = sqrt(s_0) sum e_n Y^n and
    // s_eta / sum e_n Y^n = sum g_n Y^n, Phi_eta = sqrt(Y / (2 s_0)) sum g_n Y^n / (2n + 1), and
    //     Phi_1(Y) = (1/4) sum_n Y^(n+1) / (n + 1) sum_(i+j=n) e_i g_j / (2j + 1),
    // every coefficient exact: the slope's share of the principal nears e^(Phi_1(Y)) - 1 as mu grows.
    SimRational phase_rho;
    unsigned long long phase_order = 0ull;
    std::vector<SimRational> phase_y;
    const int phase = run_cfg_rational(&cfg, "phase.rho", &phase_rho) && (sim_rational_sign(sim_rational_difference(phase_rho, one)) > 0) &&
                      run_cfg_count(&cfg, "phase.order", &phase_order) && (phase_order >= 2ull) && run_cfg_rationals(&cfg, "phase.y", &phase_y);
    if (phase)
    {
        // the cfg's order is a count of orders, far inside an unsigned int
        const unsigned int reach = (unsigned int)phase_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, angular, axial, pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(phase_rho, sim_rational_reciprocal(phase_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        // L_eta = -4 h eta and d_eta = -2 eta, and U_eta = (1 / a) U_xi
        const SimRational l_eta = core_radius_times(core_radius_times(core_radius_number(-4ll, 1ll), h), eta_v);
        const SimRational d_eta = core_radius_times(core_radius_number(-2ll, 1ll), eta_v);
        // s, the transport's coefficient, and its eta-derivative, each as a series in Y
        CoreRadiusSequence transport;
        CoreRadiusSequence transport_eta;
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            const SimRational along = core_radius_value(base.g[power], xi_v);
            const SimRational along_eta = core_radius_over(core_radius_value(core_radius_slope(base.g[power]), xi_v), cut);
            SimRational inner = core_radius_times(d_v, along);
            SimRational inner_eta = core_radius_plus(core_radius_times(d_eta, along), core_radius_times(d_v, along_eta));
            if (power == 0u)
            {
                inner = core_radius_plus(inner, core_radius_times(scale.d, eta_v));
                inner_eta = core_radius_plus(inner_eta, scale.d);
            }
            transport.push_back(core_radius_times(l_v, inner));
            transport_eta.push_back(core_radius_plus(core_radius_times(l_eta, inner), core_radius_times(l_v, inner_eta)));
        }
        const CoreRadiusSequence root = core_tangent_root(transport, reach);
        const CoreRadiusSequence quotient = core_tangent_quotient(transport_eta, root, reach);
        int multiplied = 1;
        CoreRadiusSequence phi_eta;
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            const SimRational square_gap = sim_rational_difference(core_tangent_cauchy(root, root, power), core_radius_over(transport[power], transport[0]));
            const SimRational product_gap = sim_rational_difference(core_tangent_cauchy(root, quotient, power), transport_eta[power]);
            multiplied = multiplied && (sim_rational_sign(square_gap) == 0) && (sim_rational_sign(product_gap) == 0);
            // the power is at most the cfg's order, far inside a long long
            phi_eta.push_back(core_radius_over(quotient[power], core_radius_number(2ll * (long long)power + 1ll, 1ll)));
        }
        sim_check(&results, multiplied, "the root squared is s / s_0 and the root times the quotient is s_eta at every order");
        CoreRadiusSequence phi_slope = {core_radius_number(0ll, 1ll)};
        for (unsigned int power = 1u; power <= reach; power += 1u)
        {
            // the power is at most the cfg's order, far inside a long long
            phi_slope.push_back(core_radius_over(core_tangent_cauchy(root, phi_eta, power - 1u), core_radius_number(4ll * (long long)power, 1ll)));
        }
        if (record != NULL)
        {
            record_text(record, ("phase at the vertex eta " + term_book_rational(eta_v) + " of E_rho, rho " + term_book_rational(phase_rho) + ", to order " +
                                 std::to_string(reach) + ": s_0 " + term_book_rational(transport[0]) + ", s_eta at Y = 0 " + term_book_rational(transport_eta[0]) +
                                 "; then for each n the coefficient of Y^n of Phi_1")
                                    .c_str());
            for (unsigned int power = 1u; power <= reach; power += 1u)
            {
                record_text(record, ("  " + std::to_string(power) + " " + term_book_rational(phi_slope[power])).c_str());
            }
            record_text(record, "Y, Phi_1(Y) and the last order's term over the sum");
            for (const SimRational &y_point : phase_y)
            {
                SimRational last_power = one;
                for (unsigned int power = 0u; power < reach; power += 1u)
                {
                    last_power = core_radius_times(last_power, y_point);
                }
                const SimRational sum = core_tangent_sum_at(phi_slope, y_point);
                record_text(record, ("  " + term_book_rational(y_point) + " " + term_book_rational(sum) + " " +
                                     term_book_rational(core_radius_over(core_radius_times(phi_slope[reach], last_power), sum)))
                                        .c_str());
            }
        }
        scriptura_text(&results.line, "  phase at the vertex written to the record\n");
        sim_flush(&results);
    }
    const int held = !run_cfg_short() && !record_short() && (g_sim_rational_wide == 0);
    scriptura_text(&results.line, held ? "  every value is exact and held in the build's width\n" : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every value held");
    const int recorded = (record != NULL) && record_close(record);
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core tangent");
}
