// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius.cu: a radius in X the axis core's series is proved to reach, its fields bounded order by order on a
// family of ellipses in eta, every bound an exact rational
#include "run_cfg.h"

#include "record.h"

// The ellipse E_rho is the image of the circle |z| = rho under eta = (z + 1/z) / 2, drawn over the segment
// [-1, 1] with its feet at eta = -1 and eta = +1, where d = 1 - eta^2 is 0. A field f = sum f_m T_m(eta), its
// Chebyshev weights given by the cfg, is held by ||f||_rho = sum |f_m| rho^m. For 0 <= h <= 1/2 and
// 1 < rho_min <= rho' < rho <= rho0:
//     ||f g|| <= ||f|| ||g||,   ||eta f|| <= rho0 ||f||,   ||d f|| <= (1 + rho0^2) / 2 ||f||,
//     ||L^-1 f|| <= l ||f||, l = 1 / ((1 - h) - h rho0^2),
//     ||f'||_rho' <= K / (rho - rho') ||f||_rho, K = rho0^2 / (rho_min^2 - 1),
//     ||d f'||_rho' <= ((1 + rho0^2) / 4) / (rho - rho') ||f||_rho,
// the third from T_m' = m U_(m-1), ||U_(m-1)||_rho' <= 2 rho'^(m+1) / (rho'^2 - 1) and m q^m <= 1 / (2 (1 - q)), the
// last from (1 - eta^2) T_m' = m (T_(m-1) - T_(m+1)) / 2: with the derivative held at the feet by d, its bound reads no
// rho_min. The order k is held on its own scale of ellipses, |f|_k = sup over rho in [rho_min, rho0) of
// ||f||_rho (rho0 - rho)^k, which gives |f g|_(j+k) <= |f|_j |g|_k, |f|_(k+1) <= Delta |f|_k with
// Delta = rho0 - rho_min, |f'|_(k+1) <= 3 K (k + 1) |f|_k and |d f'|_(k+1) <= 3 (1 + rho0^2) / 4 (k + 1) |f|_k.
// The rule of core_rule.cu then bounds a_k = |F_k|_k, u_k = |U_k|_k, p_k = |P_k|_k and w_k = |v_k|_(k+1) order by
// order from the data: the witness, exact to the order N. Each d f' is bounded both ways, by (1 + rho0^2) / 2 times
// 3 K and by the feet's 3 (1 + rho0^2) / 4, and each gives its own witness. The weights of each order kept apart, as
// magnitudes and exact, give two more, read on the ellipse only where the order is bounded.
// The proof: with B_x = max over k <= N of x_k r^k (k + 1), B_w raised to at least C_w B_u, C_w = l (2 rho0 Delta + E),
// E the constant of d f', and B_p to at least 2 r Delta B_a^2 H_(N+1) / (N + 1), two inequalities at k = N, each a sum
// of terms that fall as k grows, carry x_k <= B_x r^-k / (k + 1) from every order to the next past N. The 1 / (k + 1)
// is what the rule's 2 (k+1)(k+2) and 2 (k+1)^2 leave over the Cauchy sums, and with it no part of the proof stays
// as N grows: r is bounded by the witness alone. On E_rho the core's series is bounded by
// sum B_a (X / (r (rho0 - rho)))^k / (k + 1) and reaches every X < r (rho0 - rho).
// Checks:
// 1. Every order's weights are within the length the rule fixes, the length the proof on one ellipse reads.
// 2. Every ellipse of the cfg is one the bounds hold on.
// 3. Every bound of the witness and the proof is exact and held in the build's width.
// 4. The witness and the proof are written whole to the cfg's record, with the radius each proves.
// The request: core_radius <cfg>.
//     bash examples/navier_stokes/run.sh core_radius examples/navier_stokes/cfg/core_radius.cfg

typedef std::vector<SimRational> CoreRadiusSequence;

static SimRational core_radius_number(long long numerator, long long denominator)
{
    return sim_rational(numerator, denominator);
}

static SimRational core_radius_times(SimRational left, SimRational right)
{
    return sim_rational_product(left, right);
}

static SimRational core_radius_plus(SimRational left, SimRational right)
{
    return sim_rational_sum(left, right);
}

static SimRational core_radius_over(SimRational left, SimRational right)
{
    return sim_rational_product(left, sim_rational_reciprocal(right));
}

static SimRational core_radius_most(SimRational left, SimRational right)
{
    return (sim_rational_sign(sim_rational_difference(left, right)) >= 0) ? left : right;
}

// sum |c_m| rho^m
static SimRational core_radius_norm(const std::vector<SimRational> &weights, SimRational rho)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (const SimRational &weight : weights)
    {
        sum = core_radius_plus(sum, core_radius_times(sim_rational_absolute(weight), power));
        power = core_radius_times(power, rho);
    }
    return sum;
}

// the constants of one ellipse and one bound of d f'
typedef struct
{
    SimRational h;
    SimRational rho;
    SimRational delta;
    SimRational l;
    SimRational k3;
    SimRational d;
    SimRational a;
    SimRational feet;
} CoreRadiusScale;

// l [(c + 2 j) rho0 Delta + E (j + 1)], the bound of (Z_b f)_j with |2 b| = c
static SimRational core_radius_turned(const CoreRadiusScale *scale, SimRational c, SimRational j)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational eta_part = core_radius_times(core_radius_plus(c, core_radius_times(two, j)), core_radius_times(scale->rho, scale->delta));
    return core_radius_times(scale->l, core_radius_plus(eta_part, core_radius_times(scale->feet, core_radius_plus(j, one))));
}

// the witness: a, u, p and w to the order `order`
static void core_radius_witness(const CoreRadiusScale *scale, SimRational a0, SimRational u0, SimRational p0, unsigned int order,
                                CoreRadiusSequence *a, CoreRadiusSequence *u, CoreRadiusSequence *p, CoreRadiusSequence *w)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_plus(one, core_radius_times(two, scale->h));
    const SimRational pressure_c = core_radius_plus(two, core_radius_times(core_radius_number(4ll, 1ll), scale->h));
    const SimRational d_rho_k3 = core_radius_times(scale->d, core_radius_times(scale->rho, scale->k3));
    *a = {a0};
    *u = {u0};
    *p = {p0};
    w->clear();
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        // (k + 1) w_k = l [(2A + 2k) rho0 Delta + E (k + 1)] u_k
        w->push_back(core_radius_over(core_radius_times(core_radius_turned(scale, core_radius_times(two, scale->a), order_k), (*u)[k]), next));
        // L^-1 ((k + 1 + h) F_k + D eta F_k') and L^-1 ((k + A) U_k + D eta U_k')
        SimRational angular = core_radius_times(scale->l, core_radius_plus(core_radius_times(core_radius_plus(next, scale->h), scale->delta), core_radius_times(d_rho_k3, next)));
        angular = core_radius_times(angular, (*a)[k]);
        SimRational along = core_radius_times(scale->l, core_radius_plus(core_radius_times(core_radius_plus(order_k, scale->a), scale->delta), core_radius_times(d_rho_k3, next)));
        along = core_radius_times(along, (*u)[k]);
        SimRational square = core_radius_number(0ll, 1ll);
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const SimRational rest = core_radius_number((long long)(k - i), 1ll);
            // v_i (k - i + 1) F_(k-i) + U_i (Z_{-(A+1/2)} F)_(k-i)
            angular = core_radius_plus(angular, core_radius_times(core_radius_times(core_radius_plus(rest, one), (*w)[i]), (*a)[k - i]));
            angular = core_radius_plus(angular, core_radius_times((*u)[i], core_radius_times(core_radius_turned(scale, angular_c, rest), (*a)[k - i])));
            // v_i (k - i) U_(k-i) + U_i (Z_{-A} U)_(k-i)
            along = core_radius_plus(along, core_radius_times(core_radius_times(rest, (*w)[i]), (*u)[k - i]));
            along = core_radius_plus(along, core_radius_times((*u)[i], core_radius_times(core_radius_turned(scale, along_c, rest), (*u)[k - i])));
            square = core_radius_plus(square, core_radius_times((*a)[i], (*a)[k - i]));
        }
        // (Z_{-2A} Pi)_k
        along = core_radius_plus(along, core_radius_times(core_radius_turned(scale, pressure_c, order_k), (*p)[k]));
        a->push_back(core_radius_over(angular, core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))));
        u->push_back(core_radius_over(along, core_radius_times(two, core_radius_times(next, next))));
        p->push_back(core_radius_over(core_radius_times(scale->delta, square), next));
    }
}

// The weights of one order kept apart, entry m bounding |c_m| of T_m in a numerator over a power of L: no ellipse is
// carried through the orders, and one is read only where the order is bounded.
typedef std::vector<SimRational> CoreRadiusWeights;

static void core_radius_add(CoreRadiusWeights *into, size_t m, SimRational value)
{
    if (into->size() <= m)
    {
        into->resize(m + 1u, core_radius_number(0ll, 1ll));
    }
    (*into)[m] = core_radius_plus((*into)[m], value);
}

static CoreRadiusWeights core_radius_sum(const CoreRadiusWeights &left, const CoreRadiusWeights &right)
{
    CoreRadiusWeights sum = left;
    for (size_t m = 0u; m < right.size(); m += 1u)
    {
        core_radius_add(&sum, m, right[m]);
    }
    return sum;
}

static CoreRadiusWeights core_radius_scaled(const CoreRadiusWeights &weights, SimRational factor)
{
    CoreRadiusWeights scaled;
    for (const SimRational &weight : weights)
    {
        scaled.push_back(core_radius_times(weight, factor));
    }
    return scaled;
}

// eta T_0 = T_1, eta T_m = (T_(m+1) + T_(m-1)) / 2
static CoreRadiusWeights core_radius_eta(const CoreRadiusWeights &weights)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights eta;
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        if (m == 0u)
        {
            core_radius_add(&eta, 1u, weights[0]);
        }
        else
        {
            core_radius_add(&eta, m + 1u, core_radius_times(half, weights[m]));
            core_radius_add(&eta, m - 1u, core_radius_times(half, weights[m]));
        }
    }
    return eta;
}

// T_2 T_m = (T_(m+2) + T_|m-2|) / 2
static CoreRadiusWeights core_radius_second(const CoreRadiusWeights &weights)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights second;
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        core_radius_add(&second, m + 2u, core_radius_times(half, weights[m]));
        core_radius_add(&second, (m >= 2u) ? m - 2u : 2u - m, core_radius_times(half, weights[m]));
    }
    return second;
}

// L = (1 - h) - h T_2. With s = -1 the weights are a numerator's own and every operation is exact; with s = +1 they
// are magnitudes, each minus held as a plus, and the result bounds the magnitudes of the exact one.
static CoreRadiusWeights core_radius_l(const CoreRadiusWeights &weights, SimRational h, SimRational s)
{
    return core_radius_sum(core_radius_scaled(weights, sim_rational_difference(core_radius_number(1ll, 1ll), h)), core_radius_scaled(core_radius_second(weights), core_radius_times(s, h)));
}

// (1 + s T_2) / 2: d at s = -1, eta^2 at s = +1, and d held as eta^2 for magnitudes
static CoreRadiusWeights core_radius_d(const CoreRadiusWeights &weights, SimRational s)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    return core_radius_scaled(core_radius_sum(weights, core_radius_scaled(core_radius_second(weights), s)), half);
}

// T_m' = 2 m (T_(m-1) + T_(m-3) + ...), T_0 taken once where it is reached
static CoreRadiusWeights core_radius_slope(const CoreRadiusWeights &weights)
{
    CoreRadiusWeights slope;
    for (size_t m = 1u; m < weights.size(); m += 1u)
    {
        const SimRational twice = core_radius_times(core_radius_number(2ll * (long long)m, 1ll), weights[m]);
        for (size_t j = m - 1u;; j -= 2u)
        {
            core_radius_add(&slope, j, (j == 0u) ? core_radius_times(core_radius_number(1ll, 2ll), twice) : twice);
            if (j < 2u)
            {
                break;
            }
        }
    }
    return slope;
}

// d T_m' = m (T_(m-1) - T_(m+1)) / 2: at the feet the derivative moves weight one mode each way
static CoreRadiusWeights core_radius_feet_slope(const CoreRadiusWeights &weights, SimRational s)
{
    CoreRadiusWeights slope;
    for (size_t m = 1u; m < weights.size(); m += 1u)
    {
        const SimRational half = core_radius_times(core_radius_number((long long)m, 2ll), weights[m]);
        core_radius_add(&slope, m - 1u, half);
        core_radius_add(&slope, m + 1u, core_radius_times(s, half));
    }
    return slope;
}

// T_a T_b = (T_(a+b) + T_|a-b|) / 2
static CoreRadiusWeights core_radius_product(const CoreRadiusWeights &left, const CoreRadiusWeights &right)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights product;
    for (size_t m = 0u; m < left.size(); m += 1u)
    {
        if (sim_rational_sign(left[m]) == 0)
        {
            continue;
        }
        for (size_t n = 0u; n < right.size(); n += 1u)
        {
            const SimRational part = core_radius_times(half, core_radius_times(left[m], right[n]));
            core_radius_add(&product, m + n, part);
            core_radius_add(&product, (m >= n) ? m - n : n - m, part);
        }
    }
    return product;
}

// the numerator of (Z_b g)_j over L^(2j+2) for g_j over L^(2j), c = -2 b > 0:
// -(c + 2 j) eta L g + L d g' + 8 h j eta d g, its first sign s
static CoreRadiusWeights core_radius_turned_weights(const CoreRadiusWeights &g, SimRational c, unsigned int j, SimRational h, SimRational s)
{
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    CoreRadiusWeights turned = core_radius_scaled(core_radius_eta(core_radius_l(g, h, s)), core_radius_times(s, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j))));
    turned = core_radius_sum(turned, core_radius_l(core_radius_feet_slope(g, s), h, s));
    return core_radius_sum(turned, core_radius_scaled(core_radius_eta(core_radius_d(g, s)), core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), h), order_j)));
}

// The witness by weights: with F_k = f_k / L^(2k), U_k = u_k / L^(2k), P_k = p_k / L^(2k) and v_k = w_k / L^(2k+2),
// (L^-n)' = 4 h n eta L^(-n-1) and the system of core_rule.cu at order k times L^(2k+2) read
//     2 (k+1)(k+2) f_(k+1) = (k + 1 + h) L f_k + D eta L f_k' + 8 h k D eta^2 f_k + sum (k-i+1) w_i f_j
//                            + sum u_i [-(2 + 2h + 2j) eta L f_j + L d f_j' + 8 h j eta d f_j],
//     2 (k+1)^2 u_(k+1) = (k + A) L u_k + D eta L u_k' + 8 h k D eta^2 u_k + sum (k-i) w_i u_j
//                         + sum u_i [-(2A + 2j) eta L u_j + L d u_j' + 8 h j eta d u_j]
//                         - (4A + 2k) eta L p_k + L d p_k' + 8 h k eta d p_k,
//     (k+1) w_k = (2A + 2k) eta L u_k - L d u_k' - 8 h k eta d u_k,   (k+1) p_(k+1) = L^2 sum f_i f_j,
// j = k - i. ||L^-1||_rho <= l bounds each order on E_rho, and |F_k|_k <= ||F_k||_rho0 Delta^k, ||.||_rho rising with
// rho, gives the scalars the proof reads.
typedef struct
{
    std::vector<CoreRadiusWeights> f;
    std::vector<CoreRadiusWeights> g;
    std::vector<CoreRadiusWeights> q;
    std::vector<CoreRadiusWeights> inflow;
} CoreRadiusOrders;

// the weights of every order to `order`, exact at s = -1 and magnitudes at s = +1, read on no ellipse: they depend on
// h alone
static void core_radius_weights(const CoreRadiusScale *scale, const CoreRadiusWeights &angular, const CoreRadiusWeights &axial,
                                const CoreRadiusWeights &pressure, unsigned int order, SimRational s, CoreRadiusOrders *orders)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), scale->h);
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_times(two, scale->a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale->a);
    const int exact = sim_rational_sign(s) < 0;
    std::vector<CoreRadiusWeights> &f = orders->f;
    std::vector<CoreRadiusWeights> &g = orders->g;
    std::vector<CoreRadiusWeights> &q = orders->q;
    std::vector<CoreRadiusWeights> &inflow = orders->inflow;
    f = {CoreRadiusWeights()};
    g = {CoreRadiusWeights()};
    q = {CoreRadiusWeights()};
    inflow.clear();
    for (const SimRational &weight : angular)
    {
        f[0].push_back(exact ? weight : sim_rational_absolute(weight));
    }
    for (const SimRational &weight : axial)
    {
        g[0].push_back(exact ? weight : sim_rational_absolute(weight));
    }
    for (const SimRational &weight : pressure)
    {
        q[0].push_back(exact ? weight : sim_rational_absolute(weight));
    }
    std::vector<CoreRadiusWeights> angular_turned;
    std::vector<CoreRadiusWeights> along_turned;
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        along_turned.push_back(core_radius_turned_weights(g[k], along_c, k, scale->h, s));
        inflow.push_back(core_radius_scaled(along_turned[k], core_radius_times(s, sim_rational_reciprocal(next))));
        angular_turned.push_back(core_radius_turned_weights(f[k], angular_c, k, scale->h, s));
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale->d);
        CoreRadiusWeights spin = core_radius_scaled(core_radius_l(f[k], scale->h, s), core_radius_plus(next, scale->h));
        spin = core_radius_sum(spin, core_radius_scaled(core_radius_eta(core_radius_l(core_radius_slope(f[k]), scale->h, s)), scale->d));
        spin = core_radius_sum(spin, core_radius_scaled(core_radius_d(f[k], one), d_eight));
        CoreRadiusWeights along = core_radius_scaled(core_radius_l(g[k], scale->h, s), core_radius_plus(order_k, scale->a));
        along = core_radius_sum(along, core_radius_scaled(core_radius_eta(core_radius_l(core_radius_slope(g[k]), scale->h, s)), scale->d));
        along = core_radius_sum(along, core_radius_scaled(core_radius_d(g[k], one), d_eight));
        along = core_radius_sum(along, core_radius_turned_weights(q[k], pressure_c, k, scale->h, s));
        CoreRadiusWeights square;
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            const SimRational rest = core_radius_number((long long)j, 1ll);
            spin = core_radius_sum(spin, core_radius_scaled(core_radius_product(inflow[i], f[j]), core_radius_plus(rest, one)));
            spin = core_radius_sum(spin, core_radius_product(g[i], angular_turned[j]));
            along = core_radius_sum(along, core_radius_scaled(core_radius_product(inflow[i], g[j]), rest));
            along = core_radius_sum(along, core_radius_product(g[i], along_turned[j]));
            square = core_radius_sum(square, core_radius_product(f[i], f[j]));
        }
        f.push_back(core_radius_scaled(spin, sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one)))));
        g.push_back(core_radius_scaled(along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next)))));
        q.push_back(core_radius_scaled(core_radius_l(core_radius_l(square, scale->h, s), scale->h, s), sim_rational_reciprocal(next)));
    }
}

// the scalars the proof reads on one ellipse: l^(2k) ||.||_rho0 Delta^k, and l^(2k+2) ||w_k||_rho0 Delta^(k+1)
static void core_radius_weights_witness(const CoreRadiusScale *scale, const CoreRadiusOrders *orders, unsigned int order, CoreRadiusSequence *a,
                                        CoreRadiusSequence *u, CoreRadiusSequence *p, CoreRadiusSequence *w)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const std::vector<CoreRadiusWeights> &f = orders->f;
    const std::vector<CoreRadiusWeights> &g = orders->g;
    const std::vector<CoreRadiusWeights> &q = orders->q;
    const std::vector<CoreRadiusWeights> &inflow = orders->inflow;
    a->clear();
    u->clear();
    p->clear();
    w->clear();
    SimRational power = one;
    const SimRational l_square_delta = core_radius_times(core_radius_times(scale->l, scale->l), scale->delta);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        a->push_back(core_radius_times(power, core_radius_norm(f[k], scale->rho)));
        u->push_back(core_radius_times(power, core_radius_norm(g[k], scale->rho)));
        p->push_back(core_radius_times(power, core_radius_norm(q[k], scale->rho)));
        w->push_back(core_radius_times(core_radius_times(power, l_square_delta), core_radius_norm(inflow[k], scale->rho)));
        power = core_radius_times(power, l_square_delta);
    }
}

// B_x = max over k <= order of x_k r^k (k + 1)
static SimRational core_radius_most_scaled(const CoreRadiusSequence &sequence, SimRational r, unsigned int order)
{
    SimRational most = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        most = core_radius_most(most, core_radius_times(core_radius_times(sequence[k], power), core_radius_number((long long)k + 1ll, 1ll)));
        power = core_radius_times(power, r);
    }
    return most;
}

// H_n = 1 + 1/2 + ... + 1/n
static SimRational core_radius_harmonic(unsigned int n)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    for (unsigned int j = 1u; j <= n; j += 1u)
    {
        sum = core_radius_plus(sum, core_radius_number(1ll, (long long)j));
    }
    return sum;
}

// 1 where the proof carries x_k <= B_x r^-k / (k + 1) from the order `order` to every order past it at the rate r.
// With j = k - i, sum_i 1 / (i + 1) = H_(k+1), sum_i (k - i) / ((i + 1)(j + 1)) <= H_(k+1),
// sum_i 1 / ((i + 1)(j + 1)) = 2 H_(k+1) / (k + 2) and (c + 2 j) / (j + 1) <= max(c, 2), and every part below is a
// constant times 1 / (N + 1), H_(N+1) / (N + 1), (N + 2) / (N + 1)^2 or H_(N+1) (N + 2) / (N + 1)^2, each falling as N
// grows.
static int core_radius_proof(const CoreRadiusScale *scale, const CoreRadiusSequence &a, const CoreRadiusSequence &u, const CoreRadiusSequence &p,
                             const CoreRadiusSequence &w, unsigned int order, SimRational r)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational four = core_radius_number(4ll, 1ll);
    const SimRational rho_delta = core_radius_times(scale->rho, scale->delta);
    const SimRational n1 = core_radius_number((long long)order + 1ll, 1ll);
    const SimRational n2 = core_radius_plus(n1, one);
    const SimRational harmonic = core_radius_harmonic(order + 1u);
    const SimRational b_a = core_radius_most_scaled(a, r, order);
    const SimRational b_u = core_radius_most_scaled(u, r, order);
    if (sim_rational_sign(b_u) <= 0)
    {
        return 0;
    }
    // w_k <= C_w u_k past N with A <= 1, and P_(k+1) past N needs B_p >= 2 r Delta B_a^2 H_(N+1) / (N + 1)
    const SimRational c_w = core_radius_times(scale->l, core_radius_plus(core_radius_times(two, rho_delta), scale->feet));
    const SimRational b_w = core_radius_most(core_radius_most_scaled(w, r, order), core_radius_times(c_w, b_u));
    const SimRational pressure_floor = core_radius_over(core_radius_times(core_radius_times(two, r), core_radius_times(scale->delta, core_radius_times(core_radius_times(b_a, b_a), harmonic))), n1);
    const SimRational b_p = core_radius_most(core_radius_most_scaled(p, r, order), pressure_floor);
    const SimRational d_rho_k3 = core_radius_times(scale->d, core_radius_times(scale->rho, scale->k3));
    const SimRational falling = sim_rational_reciprocal(n1);
    const SimRational harmonic_falling = core_radius_times(harmonic, falling);
    const SimRational square_falling = core_radius_over(n2, core_radius_times(n1, n1));
    const SimRational harmonic_square_falling = core_radius_times(harmonic, square_falling);
    // the angular part: l [(1 + h) Delta + D rho0 3K] / (2 (N + 1))
    //                   + (B_w + B_u l ((2 + 2h) rho0 Delta + E)) H_(N+1) / (2 (N + 1)) <= 1 / r
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    SimRational angular = core_radius_times(core_radius_times(scale->l, core_radius_plus(core_radius_times(core_radius_plus(one, scale->h), scale->delta), d_rho_k3)), falling);
    const SimRational angular_sum = core_radius_plus(b_w, core_radius_times(b_u, core_radius_times(scale->l, core_radius_plus(core_radius_times(angular_c, rho_delta), scale->feet))));
    angular = core_radius_over(core_radius_plus(angular, core_radius_times(angular_sum, harmonic_falling)), two);
    // the axial part: (l (Delta + D rho0 3K) + l ((2 + 4h) rho0 Delta + E) B_p / B_u) (N + 2) / (2 (N + 1)^2)
    //                 + (B_w + B_u l (2 rho0 Delta + E)) H_(N+1) (N + 2) / (2 (N + 1)^2) <= 1 / r
    const SimRational pressure_c = core_radius_plus(two, core_radius_times(four, scale->h));
    SimRational along = core_radius_times(scale->l, core_radius_plus(scale->delta, d_rho_k3));
    along = core_radius_plus(along, core_radius_over(core_radius_times(core_radius_times(scale->l, core_radius_plus(core_radius_times(pressure_c, rho_delta), scale->feet)), b_p), b_u));
    along = core_radius_times(along, square_falling);
    along = core_radius_plus(along, core_radius_times(core_radius_plus(b_w, core_radius_times(b_u, c_w)), harmonic_square_falling));
    along = core_radius_over(along, two);
    const SimRational limit = sim_rational_reciprocal(r);
    return (sim_rational_sign(sim_rational_difference(limit, angular)) >= 0) && (sim_rational_sign(sim_rational_difference(limit, along)) >= 0);
}

// The proof on one ellipse. Every term of the rule raises a numerator's degree by at most 3, and a product of two by
// at most 4: f_k, u_k and p_k have degree at most G_k = g0 + (g0 + 4) k and w_k at most G_k + 3, g0 the data's
// degree: the length of each order's weights is fixed by the rule, and on E_rho a derivative costs that length,
// ||g'|| <= gamma G ||g||, gamma = 2 rho / (rho^2 - 1), and ||d g'|| <= phi G ||g||, phi = (rho + 1/rho) / 2. With
// lambda = (1 - h) + h rho^2, delta = (1 + rho^2) / 2 and beta = 2 rho lambda + lambda phi (g0 + 4) + 8 h rho delta,
// the Z parts of order j are at most (alpha + beta j) times their field, alpha = c rho lambda + lambda phi g0. The
// Cauchy sums split at N0: a part with one index below N0 reads the witness exactly, and only a part with both indices
// at N0 or past it reads the bound. With n_k <= B r^-k for every k >= N0, B = max over N0 <= k <= N of n_k r^k, two
// inequalities at k = N carry the bound to every order past N, and ||F_k||_rho <= l^(2k) B_f r^-k reaches every
// X < r / l^2 on E_rho with no ellipse lost.
typedef struct
{
    SimRational lambda;
    SimRational delta;
    SimRational gamma;
    SimRational phi;
    SimRational beta;
    SimRational alpha_angular;
    SimRational alpha_along;
    SimRational alpha_pressure;
} CoreRadiusFixed;

// sum over k < split of weight(k) n_k r^k
static SimRational core_radius_low(const CoreRadiusSequence &n, SimRational r, unsigned int split, SimRational alpha, SimRational beta)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k < split; k += 1u)
    {
        const SimRational weight = core_radius_plus(alpha, core_radius_times(beta, core_radius_number((long long)k, 1ll)));
        sum = core_radius_plus(sum, core_radius_times(weight, core_radius_times(n[k], power)));
        power = core_radius_times(power, r);
    }
    return sum;
}

// max over split <= k <= order of n_k r^k
static SimRational core_radius_high(const CoreRadiusSequence &n, SimRational r, unsigned int split, unsigned int order)
{
    SimRational most = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        if (k >= split)
        {
            most = core_radius_most(most, core_radius_times(n[k], power));
        }
        power = core_radius_times(power, r);
    }
    return most;
}

// 1 where the proof on one ellipse carries n_k <= B r^-k from N to every order past it
static int core_radius_fixed_proof(const CoreRadiusScale *scale, const CoreRadiusFixed *fixed, unsigned int g0, const CoreRadiusSequence &nf,
                                   const CoreRadiusSequence &nu, const CoreRadiusSequence &np, const CoreRadiusSequence &nw, unsigned int order,
                                   unsigned int split, SimRational r)
{
    const SimRational zero = core_radius_number(0ll, 1ll);
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational quarter = core_radius_number(1ll, 4ll);
    const SimRational n = core_radius_number((long long)order, 1ll);
    const SimRational n1 = core_radius_plus(n, one);
    const SimRational n2 = core_radius_plus(n1, one);
    const SimRational g = core_radius_number((long long)g0, 1ll);
    const SimRational g1 = core_radius_number((long long)g0 + 4ll, 1ll);
    const SimRational b_f = core_radius_high(nf, r, split, order);
    const SimRational b_u = core_radius_high(nu, r, split, order);
    if ((sim_rational_sign(b_f) <= 0) || (sim_rational_sign(b_u) <= 0))
    {
        return 0;
    }
    const SimRational f0 = core_radius_low(nf, r, split, one, zero);
    const SimRational u0 = core_radius_low(nu, r, split, one, zero);
    const SimRational w0 = core_radius_low(nw, r, split, one, zero);
    const SimRational b_w = core_radius_most(core_radius_high(nw, r, split, order), core_radius_times(core_radius_most(fixed->alpha_along, fixed->beta), b_u));
    const SimRational lambda_square = core_radius_times(fixed->lambda, fixed->lambda);
    const SimRational pressure_floor = core_radius_times(core_radius_times(r, lambda_square), core_radius_plus(core_radius_over(core_radius_times(core_radius_times(two, f0), b_f), n1), core_radius_times(b_f, b_f)));
    const SimRational b_p = core_radius_most(core_radius_high(np, r, split, order), pressure_floor);
    const SimRational d_rho_lambda_gamma = core_radius_times(scale->d, core_radius_times(scale->rho, core_radius_times(fixed->lambda, fixed->gamma)));
    const SimRational eight_h_d_delta = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), core_radius_times(scale->d, fixed->delta));
    const SimRational a1 = core_radius_plus(core_radius_plus(fixed->lambda, core_radius_times(d_rho_lambda_gamma, g1)), eight_h_d_delta);
    const SimRational angular_a0 = core_radius_plus(core_radius_times(core_radius_plus(one, scale->h), fixed->lambda), core_radius_times(d_rho_lambda_gamma, g));
    const SimRational along_a0 = core_radius_plus(core_radius_times(scale->a, fixed->lambda), core_radius_times(d_rho_lambda_gamma, g));
    const SimRational square_12 = core_radius_times(two, core_radius_times(n1, n2));
    const SimRational square_11 = core_radius_times(two, core_radius_times(n1, n1));
    // the angular part at N, each term falling as N grows:
    //   (a0 + a1 N) / (2 (N+1)(N+2)) + W0 / (2 (N+2)) + B_w F1 / (2 (N+1)(N+2) B_f) + B_w / 4
    //   + U0 (alpha + beta N) / (2 (N+1)(N+2)) + B_u F_alpha / (2 (N+1)(N+2) B_f) + B_u (alpha / (2 (N+2)) + beta / 4)
    const SimRational alpha_t = fixed->alpha_angular;
    SimRational angular = core_radius_over(core_radius_plus(angular_a0, core_radius_times(a1, n)), square_12);
    angular = core_radius_plus(angular, core_radius_over(w0, core_radius_times(two, n2)));
    angular = core_radius_plus(angular, core_radius_over(core_radius_times(b_w, core_radius_low(nf, r, split, one, one)), core_radius_times(square_12, b_f)));
    angular = core_radius_plus(angular, core_radius_times(b_w, quarter));
    angular = core_radius_plus(angular, core_radius_over(core_radius_times(u0, core_radius_plus(alpha_t, core_radius_times(fixed->beta, n))), square_12));
    angular = core_radius_plus(angular, core_radius_over(core_radius_times(b_u, core_radius_low(nf, r, split, alpha_t, fixed->beta)), core_radius_times(square_12, b_f)));
    angular = core_radius_plus(angular, core_radius_times(b_u, core_radius_plus(core_radius_over(alpha_t, core_radius_times(two, n2)), core_radius_times(fixed->beta, quarter))));
    // the axial part at N:
    //   (b0 + a1 N) / (2 (N+1)^2) + W0 / (2 (N+1)) + B_w U1 / (2 (N+1)^2 B_u) + B_w / 4
    //   + U0 (alpha + beta N) / (2 (N+1)^2) + U_alpha / (2 (N+1)^2) + B_u (alpha / (2 (N+1)) + beta / 4)
    //   + (alpha_p + beta N) B_p / (2 (N+1)^2 B_u)
    const SimRational alpha_a = fixed->alpha_along;
    SimRational along = core_radius_over(core_radius_plus(along_a0, core_radius_times(a1, n)), square_11);
    along = core_radius_plus(along, core_radius_over(w0, core_radius_times(two, n1)));
    along = core_radius_plus(along, core_radius_over(core_radius_times(b_w, core_radius_low(nu, r, split, zero, one)), core_radius_times(square_11, b_u)));
    along = core_radius_plus(along, core_radius_times(b_w, quarter));
    along = core_radius_plus(along, core_radius_over(core_radius_times(u0, core_radius_plus(alpha_a, core_radius_times(fixed->beta, n))), square_11));
    along = core_radius_plus(along, core_radius_over(core_radius_low(nu, r, split, alpha_a, fixed->beta), square_11));
    along = core_radius_plus(along, core_radius_times(b_u, core_radius_plus(core_radius_over(alpha_a, core_radius_times(two, n1)), core_radius_times(fixed->beta, quarter))));
    along = core_radius_plus(along, core_radius_over(core_radius_times(core_radius_plus(fixed->alpha_pressure, core_radius_times(fixed->beta, n)), b_p), core_radius_times(square_11, b_u)));
    const SimRational limit = sim_rational_reciprocal(r);
    return (sim_rational_sign(sim_rational_difference(limit, angular)) >= 0) && (sim_rational_sign(sim_rational_difference(limit, along)) >= 0);
}

// sum m |c_m| rho^m / (length sum |c_m| rho^m), 0 for weights all 0
static SimRational core_radius_place(const CoreRadiusWeights &weights, SimRational rho, unsigned int length)
{
    SimRational moment = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        moment = core_radius_plus(moment, core_radius_times(core_radius_number((long long)m, 1ll), core_radius_times(sim_rational_absolute(weights[m]), power)));
        power = core_radius_times(power, rho);
    }
    const SimRational mass = core_radius_times(core_radius_number((long long)length, 1ll), core_radius_norm(weights, rho));
    return (sim_rational_sign(mass) > 0) ? core_radius_over(moment, mass) : core_radius_number(0ll, 1ll);
}

// the largest r = j step the proof on E_rho holds at, the radius r / l^2 it proves, with the norms written to the record
static SimRational core_radius_fixed_rate(const CoreRadiusScale *scale, const CoreRadiusOrders *orders, unsigned int g0, unsigned int order,
                                          unsigned int split, SimRational step, unsigned long long steps, FILE *record)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational rho = scale->rho;
    const SimRational rho_square = core_radius_times(rho, rho);
    CoreRadiusFixed fixed;
    fixed.lambda = core_radius_plus(sim_rational_difference(one, scale->h), core_radius_times(scale->h, rho_square));
    fixed.delta = core_radius_over(core_radius_plus(one, rho_square), two);
    fixed.gamma = core_radius_over(core_radius_times(two, rho), sim_rational_difference(rho_square, one));
    fixed.phi = core_radius_over(core_radius_plus(rho, sim_rational_reciprocal(rho)), two);
    const SimRational rho_lambda = core_radius_times(rho, fixed.lambda);
    const SimRational lambda_phi = core_radius_times(fixed.lambda, fixed.phi);
    const SimRational g = core_radius_number((long long)g0, 1ll);
    fixed.beta = core_radius_plus(core_radius_plus(core_radius_times(two, rho_lambda), core_radius_times(lambda_phi, core_radius_number((long long)g0 + 4ll, 1ll))),
                                  core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), core_radius_times(rho, fixed.delta)));
    fixed.alpha_angular = core_radius_plus(core_radius_times(core_radius_plus(two, core_radius_times(two, scale->h)), rho_lambda), core_radius_times(lambda_phi, g));
    fixed.alpha_along = core_radius_plus(core_radius_times(core_radius_times(two, scale->a), rho_lambda), core_radius_times(lambda_phi, g));
    fixed.alpha_pressure = core_radius_plus(core_radius_times(core_radius_times(core_radius_number(4ll, 1ll), scale->a), rho_lambda), core_radius_times(lambda_phi, g));
    CoreRadiusSequence nf;
    CoreRadiusSequence nu;
    CoreRadiusSequence np;
    CoreRadiusSequence nw;
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        nf.push_back(core_radius_norm(orders->f[k], rho));
        nu.push_back(core_radius_norm(orders->g[k], rho));
        np.push_back(core_radius_norm(orders->q[k], rho));
        nw.push_back(core_radius_norm(orders->inflow[k], rho));
    }
    unsigned long long low = 0ull;
    unsigned long long high = steps + 1ull;
    while (high - low > 1ull)
    {
        const unsigned long long middle = low + (high - low) / 2ull;
        if (core_radius_fixed_proof(scale, &fixed, g0, nf, nu, np, nw, order, split, core_radius_times(core_radius_number((long long)middle, 1ll), step)))
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    const SimRational proved = core_radius_times(core_radius_number((long long)low, 1ll), step);
    const SimRational radius = core_radius_over(proved, core_radius_times(scale->l, scale->l));
    if (record != NULL)
    {
        record_text(record, ("  one ellipse, split " + std::to_string(split) + ": ||f_k||, ||u_k||, ||p_k||, ||w_k|| on E_rho0 to order " + std::to_string(order)).c_str());
        for (unsigned int k = 0u; k <= order; k += 1u)
        {
            record_text(record, ("    " + term_book_rational(nf[k]) + " " + term_book_rational(nu[k]) + " " + term_book_rational(np[k]) + " " + term_book_rational(nw[k])).c_str());
        }
        record_text(record, ("  proved r " + term_book_rational(proved) + ", radius " + term_book_rational(radius)).c_str());
        // each order's weights read as amplitudes in [0, 1] over a place t = m / G_k in [0, 1]: where the order holds
        // its weight, and so the share of its length a derivative of it costs
        record_text(record, "  place of f_k and u_k on the 0-1 scale, sum m |c_m| rho^m / (G_k sum |c_m| rho^m)");
        for (unsigned int k = 1u; k <= order; k += 1u)
        {
            const unsigned int length = g0 + (g0 + 4u) * k;
            record_text(record, ("    " + std::to_string(k) + " " + term_book_rational(core_radius_place(orders->f[k], rho, length)) + " " +
                                 term_book_rational(core_radius_place(orders->g[k], rho, length)))
                                    .c_str());
        }
    }
    return radius;
}

// the largest r = j step, j = 1..steps, the proof holds at over the witness a, u, p, w, 0 where it holds at none, with
// the witness and r written to the record under `name`
static SimRational core_radius_rate(const CoreRadiusScale *scale, const CoreRadiusSequence &a, const CoreRadiusSequence &u, const CoreRadiusSequence &p,
                                    const CoreRadiusSequence &w, unsigned int order, SimRational step, unsigned long long steps, FILE *record,
                                    const std::string &name)
{
    // every B_x grows with r and 1 / r falls: where the proof holds at r it holds below r, and j is bisected
    unsigned long long low = 0ull;
    unsigned long long high = steps + 1ull;
    while (high - low > 1ull)
    {
        const unsigned long long middle = low + (high - low) / 2ull;
        if (core_radius_proof(scale, a, u, p, w, order, core_radius_times(core_radius_number((long long)middle, 1ll), step)))
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    const SimRational proved = core_radius_times(core_radius_number((long long)low, 1ll), step);
    if (record != NULL)
    {
        record_text(record, ("  " + name + ", tail E " + term_book_rational(scale->feet) + ": a, u, p, w to order " + std::to_string(order)).c_str());
        for (unsigned int k = 0u; k <= order; k += 1u)
        {
            record_text(record, ("    " + term_book_rational(a[k]) + " " + term_book_rational(u[k]) + " " + term_book_rational(p[k]) + " " + term_book_rational(w[k])).c_str());
        }
        record_text(record, ("  proved r " + term_book_rational(proved) + ", radius " + term_book_rational(core_radius_times(proved, scale->delta))).c_str());
    }
    return proved;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    std::vector<SimRational> angular;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    std::vector<SimRational> outer;
    std::vector<SimRational> inner;
    unsigned long long order = 0ull;
    unsigned long long steps = 0ull;
    unsigned long long split = 0ull;
    SimRational step;
    const int read = (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) && run_cfg_rational(&cfg, "core.anisotropy", &h) &&
                     run_cfg_rationals(&cfg, "core.axis.angular", &angular) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
                     run_cfg_rationals(&cfg, "core.axis.pressure", &pressure) && run_cfg_rationals(&cfg, "ellipse.outer", &outer) &&
                     run_cfg_rationals(&cfg, "ellipse.inner", &inner) && (outer.size() == inner.size()) && run_cfg_count(&cfg, "order", &order) &&
                     (order >= 1ull) && run_cfg_rational(&cfg, "rate.step", &step) && (sim_rational_sign(step) > 0) &&
                     run_cfg_count(&cfg, "rate.steps", &steps) && (steps >= 1ull) && run_cfg_count(&cfg, "split", &split) && (split >= 1ull) &&
                     (order >= 2ull * split);
    if (!read)
    {
        run_cfg_missing(&results.line, "core anisotropy, axis angular, axial and pressure, ellipse outer and inner of one length, order, rate "
                                       "step and steps, and a split at least 1 with order at least twice it");
        sim_flush(&results);
        fprintf(stderr, "core_radius <cfg>\n");
        return 2;
    }
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational half = core_radius_number(1ll, 2ll);
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int legal = (sim_rational_sign(h) >= 0) && (sim_rational_sign(sim_rational_difference(half, h)) >= 0);
    CoreRadiusScale constants;
    constants.h = h;
    constants.d = sim_rational_difference(half, h);
    constants.a = core_radius_plus(half, h);
    CoreRadiusOrders orders;
    core_radius_weights(&constants, angular, axial, pressure, (unsigned int)order, one, &orders);
    CoreRadiusOrders exact;
    core_radius_weights(&constants, angular, axial, pressure, (unsigned int)order, core_radius_number(-1ll, 1ll), &exact);
    // g0, the data's degree, and every order's weights within the length G_k + 1 the rule fixes, w_k within G_k + 4
    const unsigned int g0 = (unsigned int)std::max(std::max(angular.size(), axial.size()), pressure.size()) - 1u;
    int fixed_length = 1;
    for (unsigned int k = 0u; k <= (unsigned int)order; k += 1u)
    {
        const size_t length = (size_t)g0 + (size_t)(g0 + 4u) * k + 1u;
        fixed_length = fixed_length && (exact.f[k].size() <= length) && (exact.g[k].size() <= length) && (exact.q[k].size() <= length) &&
                       (exact.inflow[k].size() <= length + 3u);
    }
    sim_check(&results, fixed_length, "every order's weights within the length the rule fixes");
    for (size_t index = 0u; index < outer.size(); index += 1u)
    {
        const SimRational rho_square = core_radius_times(outer[index], outer[index]);
        const SimRational l_inverse = sim_rational_difference(sim_rational_difference(one, h), core_radius_times(h, rho_square));
        const SimRational inner_square = sim_rational_difference(core_radius_times(inner[index], inner[index]), one);
        CoreRadiusScale scale;
        scale.h = h;
        scale.rho = outer[index];
        scale.delta = sim_rational_difference(outer[index], inner[index]);
        const int held = (sim_rational_sign(scale.delta) > 0) && (sim_rational_sign(l_inverse) > 0) && (sim_rational_sign(inner_square) > 0);
        legal = legal && held;
        if (!held)
        {
            continue;
        }
        scale.l = sim_rational_reciprocal(l_inverse);
        scale.k3 = core_radius_over(core_radius_times(core_radius_number(3ll, 1ll), rho_square), inner_square);
        scale.d = sim_rational_difference(half, h);
        scale.a = core_radius_plus(half, h);
        if (record != NULL)
        {
            record_text(record, ("ellipse rho0 " + term_book_rational(outer[index]) + ", rho_min " + term_book_rational(inner[index])).c_str());
        }
        // E as (1 + rho0^2) / 2 times 3 K, then as the feet's 3 (1 + rho0^2) / 4
        const unsigned int last = (unsigned int)order;
        CoreRadiusSequence a;
        CoreRadiusSequence u;
        CoreRadiusSequence p;
        CoreRadiusSequence w;
        scale.feet = core_radius_times(core_radius_over(core_radius_plus(one, rho_square), core_radius_number(2ll, 1ll)), scale.k3);
        core_radius_witness(&scale, core_radius_norm(angular, scale.rho), core_radius_norm(axial, scale.rho), core_radius_norm(pressure, scale.rho), last, &a, &u, &p, &w);
        const SimRational plain = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "bounds, d f' by K");
        scale.feet = core_radius_over(core_radius_times(core_radius_number(3ll, 1ll), core_radius_plus(one, rho_square)), core_radius_number(4ll, 1ll));
        core_radius_witness(&scale, core_radius_norm(angular, scale.rho), core_radius_norm(axial, scale.rho), core_radius_norm(pressure, scale.rho), last, &a, &u, &p, &w);
        const SimRational feet = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "bounds, d f' at the feet");
        core_radius_weights_witness(&scale, &orders, last, &a, &u, &p, &w);
        const SimRational weights = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "weights");
        core_radius_weights_witness(&scale, &exact, last, &a, &u, &p, &w);
        const SimRational kept = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "exact weights");
        const SimRational one_ellipse = core_radius_fixed_rate(&scale, &exact, g0, last, (unsigned int)split, step, steps, record);
        scriptura_text(&results.line, "  rho0 ");
        sim_rational_print(&results.line, outer[index]);
        scriptura_text(&results.line, ", rho_min ");
        sim_rational_print(&results.line, inner[index]);
        scriptura_text(&results.line, ": X < ");
        sim_rational_print(&results.line, core_radius_times(plain, scale.delta));
        scriptura_text(&results.line, " with d f' bounded by K, X < ");
        sim_rational_print(&results.line, core_radius_times(feet, scale.delta));
        scriptura_text(&results.line, " with it bounded at the feet, X < ");
        sim_rational_print(&results.line, core_radius_times(weights, scale.delta));
        scriptura_text(&results.line, " with each mode's weight kept, X < ");
        sim_rational_print(&results.line, core_radius_times(kept, scale.delta));
        scriptura_text(&results.line, " with each weight exact, X < ");
        sim_rational_print(&results.line, one_ellipse);
        scriptura_text(&results.line, " on E_rho0 alone\n");
        sim_flush(&results);
    }
    sim_check(&results, legal, "every ellipse of the cfg one the bounds hold on");
    const int held = !run_cfg_short() && !record_short() && (s_sim_rational_wide == 0);
    scriptura_text(&results.line, held ? "  every bound is exact and held in the build's width\n" : "  a bound outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every bound held");
    const int recorded = (record != NULL) && record_close(record);
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core radius");
}
