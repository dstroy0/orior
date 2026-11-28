// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_tangent.cu: the axis core's tangent along the pressure datum in the direction e^(mu eta), every order exactly e^(mu
// eta) times a polynomial in mu whose coefficients are exact Chebyshev weights in xi = eta / a
#include "run_cfg.h"

#include "record.h"

#include "core_radius_weights.h"
#include "../../../../includes/platform/platform.h"

// The datum enters the core through Z_{-2A} Pi alone, and the tangent of the rule of core_radius_weights.h along a
// datum direction p is the same rule taken once in the tangent of each field: with j = k - i,
//     2 (k+1)(k+2) df_(k+1) = (k + 1 + h) L df_k + D eta L df_k' + 8 h k D eta^2 df_k
//                             + sum (k - i + 1) (dw_i f_j + w_i df_j) + sum (du_i a_j + u_i (Z df)_j),
//     2 (k+1)^2 du_(k+1) = (k + A) L du_k + D eta L du_k' + 8 h k D eta^2 du_k
//                          + sum (k - i) (dw_i u_j + w_i du_j) + sum (du_i b_j + u_i (Z du)_j) + (Z dp)_k,
//     (k+1) dw_k = -(Z du)_k,   (k+1) dp_(k+1) = L^2 sum (df_i f_j + f_i df_j),   dp_0 = p,
// a_j and b_j the base's Z parts. Along p = e^(mu eta), d/deta (e^(mu eta) P) = e^(mu eta) (mu P + P_eta), and each
// field of order k is e^(mu eta) times a polynomial in mu, stored as one vector of weights for each power.
//
// Only a path that takes the derivative of e^(mu eta) at every order reaches the power mu^k at order k, and such a path
// reads the base at order 0 alone. The top weights therefore follow, with sigma = D eta + U_0 d,
//     du_1 = L d / 2,   du_(k+1) = sigma L du_k / (2 (k+1)^2),   dw_k = -L d du_k / (k + 1),
//     df_(k+1) = (sigma L df_k + F_0 dw_k) / (2 (k+1)(k+2)),
// each a product of exact weights: the coefficient of mu^k at order k gains a factor near sigma mu / (2 k^2) at every
// order, the loss of one eta-derivative an order, and at a point where sigma and d are past 0 it retains one sign.
// Checks:
// 1. du_k has degree k in mu from k = 1, and df_k from k = 2.
// 2. The top weights of du_k and df_k are the weights of the products above at every order to the cfg's.
// 3. At the cfg's eta, sigma and d are past 0 and the top coefficient of dF_k = df_k / L^(2k) retains one sign.
// 4. Every exact value is stored in the build's width.
// 5. Every coefficient at the cfg's eta is written whole to the cfg's record.
// 6. Where the cfg contains `apart`, the tangent with every part off is the principal system at the vertex, order by
// order.
// 7. Where the cfg contains `phase`, the root squared is s / s_0 and the root times the quotient is s_eta, at every
// order.
// 8. Where the cfg contains `phase`, the amplitude's first coefficient is s_0 (L_eta / L + d_eta / d) - (3/2) s_eta at
// Y = 0.
// 9. Where the cfg contains `phase`, the swirl's b_0 is -L d f_0 / s_0 and its first coefficient
//    (3/2) s_0 (L_eta / L + d_eta / d) + (1/2) s_0 f_eta / f_0 - 2 s_eta at Y = 0.
// The request: core_tangent <cfg>.
//     bash examples/navier_stokes/run.sh core_tangent examples/navier_stokes/cfg/core_tangent.cfg

// the bytes of the pool: the base's orders and the tangent's powers of mu at each order, at 1 KB a rational
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// an empty sequence, with no cell
#define CORE_TANGENT_NONE {NULL, 0u, 0u}

// One field of one order: entry n the weights of mu^n, `count` of them, in a cell of the temporary tier. The weights
// of each entry are a cell of that tier too. The caller gives the cells back with core_tangent_powers_released
typedef struct
{
    CoreRadiusWeights *power;
    unsigned int count;
} CoreTangentPowers;

// `powers` with `count` entries, each empty
static void core_tangent_powers_taken(CoreTangentPowers *powers, unsigned int count)
{
    powers->power = (count != 0u) ? (CoreRadiusWeights *)platform_cell_cleared(count, sizeof(CoreRadiusWeights),
                                                                               platform_place_g_program_owner, 1)
                                  : NULL;
    powers->count = (powers->power != NULL) ? count : 0u;
}

// every entry's cell and the entries' cell given back, and `powers` empty
static void core_tangent_powers_released(CoreTangentPowers *powers)
{
    for (unsigned int n = 0u; n < powers->count; n += 1u)
    {
        core_radius_released(&powers->power[n]);
    }
    platform_cell_released(powers->power, platform_place_g_program_owner);
    powers->power = NULL;
    powers->count = 0u;
}

// the cells of `into` given back, and `made` in its place
static void core_tangent_powers_replaced(CoreTangentPowers *into, CoreTangentPowers *made)
{
    core_tangent_powers_released(into);
    *into = *made;
    made->power = NULL;
    made->count = 0u;
}

// `add` summed into `into`, `add` as it was
static void core_tangent_added(CoreRadiusWeights *into, const CoreRadiusWeights *add)
{
    CoreRadiusWeights made;
    core_radius_sum(into, add, &made);
    core_radius_replaced(into, &made);
}

// `add` summed into `into`, and the cell of `add` given back
static void core_tangent_sum_into(CoreRadiusWeights *into, CoreRadiusWeights *add)
{
    core_tangent_added(into, add);
    core_radius_released(add);
}

// left + right at each power, a power that is absent read as empty; into `out`, which may be either
static void core_tangent_sum(const CoreTangentPowers *left, const CoreTangentPowers *right, CoreTangentPowers *out)
{
    const CoreRadiusWeights none = CORE_TANGENT_NONE;
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, (left->count > right->count) ? left->count : right->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_sum((n < left->count) ? &left->power[n] : &none, (n < right->count) ? &right->power[n] : &none,
                        &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

// `add` summed into `into`, and the cells of `add` given back
static void core_tangent_powers_into(CoreTangentPowers *into, CoreTangentPowers *add)
{
    core_tangent_sum(into, add, into);
    core_tangent_powers_released(add);
}

static void core_tangent_scaled(const CoreTangentPowers *powers, SimRational factor, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_scaled(&powers->power[n], factor, &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

// L, d, eta = a xi and eta^2 taken at each power, exact
static void core_tangent_l(const CoreTangentPowers *powers, const CoreRadiusScale *scale, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_l(&powers->power[n], scale, core_radius_number(-1ll, 1ll), &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

static void core_tangent_d(const CoreTangentPowers *powers, const CoreRadiusScale *scale, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_d(&powers->power[n], scale, core_radius_number(-1ll, 1ll), &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

static void core_tangent_eta(const CoreTangentPowers *powers, const CoreRadiusScale *scale, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        CoreRadiusWeights eta;
        core_radius_eta(&powers->power[n], &eta);
        core_radius_scaled(&eta, scale->cut, &made.power[n]);
        core_radius_released(&eta);
    }
    core_tangent_powers_replaced(out, &made);
}

static void core_tangent_eta_square(const CoreTangentPowers *powers, const CoreRadiusScale *scale,
                                    CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_eta_square(&powers->power[n], scale, &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

// d/deta (e^(mu eta) P) over e^(mu eta): mu P + P_eta, P_eta = (1 / a) P_xi
static void core_tangent_slope(const CoreTangentPowers *powers, const CoreRadiusScale *scale, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count + 1u);
    for (unsigned int n = 0u; (made.count != 0u) && (n < powers->count); n += 1u)
    {
        CoreRadiusWeights slope;
        CoreRadiusWeights slope_scaled;
        core_radius_slope(&powers->power[n], &slope);
        core_radius_scaled(&slope, sim_rational_reciprocal(scale->cut), &slope_scaled);
        core_radius_released(&slope);
        core_tangent_sum_into(&made.power[n], &slope_scaled);
        core_tangent_added(&made.power[n + 1u], &powers->power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

// the base's weights times each power
static void core_tangent_product(const CoreRadiusWeights *base, const CoreTangentPowers *powers, CoreTangentPowers *out)
{
    CoreTangentPowers made;
    core_tangent_powers_taken(&made, powers->count);
    for (unsigned int n = 0u; n < made.count; n += 1u)
    {
        core_radius_product(base, &powers->power[n], &made.power[n]);
    }
    core_tangent_powers_replaced(out, &made);
}

// (Z_b g)_j: -(c + 2 j) eta L g + L d g' + 8 h j eta d g, c = -2 b
static void core_tangent_turned(const CoreTangentPowers *g, SimRational c, unsigned int j, const CoreRadiusScale *scale,
                                CoreTangentPowers *out)
{
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    const SimRational eta_turn = core_radius_number(-1ll, 1ll);
    CoreTangentPowers made = {NULL, 0u};
    CoreTangentPowers part = {NULL, 0u};
    core_tangent_l(g, scale, &part);
    core_tangent_eta(&part, scale, &part);
    core_tangent_scaled(
        &part,
        core_radius_times(eta_turn, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j))),
        &made);
    core_tangent_slope(g, scale, &part);
    core_tangent_d(&part, scale, &part);
    core_tangent_l(&part, scale, &part);
    core_tangent_sum(&made, &part, &made);
    const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
    core_tangent_d(g, scale, &part);
    core_tangent_eta(&part, scale, &part);
    core_tangent_scaled(&part, eight_h_j, &part);
    core_tangent_sum(&made, &part, &made);
    core_tangent_powers_released(&part);
    core_tangent_powers_replaced(out, &made);
}

// the power of the last nonzero weights, 0 for none
static unsigned int core_tangent_degree(const CoreTangentPowers *powers)
{
    unsigned int degree = 0u;
    for (unsigned int n = 0u; n < powers->count; n += 1u)
    {
        for (unsigned int m = 0u; m < powers->power[n].count; m += 1u)
        {
            if (sim_rational_sign(powers->power[n].value[m]) != 0)
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
//     (k+1) w_k = -mu L d y_k,   2 (k+1)(k+2) g_(k+1) = mu (D eta L g_k + L d sum u_i g_(k-i)) + sum (k - i + 1) w_i
//     f_(k-i),
// u_i and f_i the base's numerators at the point. Its growth along Y = X / L^2 is e^(sqrt(mu) Phi(Y)), Phi the integral
// of sqrt(s / (2 Y)), s = L (D eta + d U(Y)) = sum s_k Y^k: with sqrt(s / s_0) = sum e_k Y^k,
//     Phi(Y) = sqrt(2 s_0 Y) sum e_k Y^k / (2 k + 1),
// every e_k exact. The record contains each value whole.

// The parts of the full tangent the principal system leaves out: the slope, P_eta in d/deta (e^(mu eta) P). The plain
// terms, every term that takes no derivative; and the pressure, dp_k past k = 0. With every part off the full tangent is
// the principal one, and with every part on it is the full one.
typedef struct
{
    int slope;
    int plain;
    int pressure;
} CoreTangentParts;

// mu P + P_eta for mu fixed, P_eta where the slope is on
static void core_tangent_slope_at(const CoreRadiusWeights *weights, SimRational mu, const CoreRadiusScale *scale,
                                  const CoreTangentParts *parts, CoreRadiusWeights *out)
{
    core_radius_scaled(weights, mu, out);
    if (parts->slope)
    {
        CoreRadiusWeights slope;
        CoreRadiusWeights slope_scaled;
        core_radius_slope(weights, &slope);
        core_radius_scaled(&slope, sim_rational_reciprocal(scale->cut), &slope_scaled);
        core_radius_released(&slope);
        core_tangent_sum_into(out, &slope_scaled);
    }
}

// (Z_b g)_j for mu fixed: L d (mu g + g_eta), and the plain -(c + 2 j) eta L g + 8 h j eta d g where the plain terms are on
static void core_tangent_turned_at(const CoreRadiusWeights *g, SimRational c, unsigned int j, SimRational mu,
                                   const CoreRadiusScale *scale, const CoreTangentParts *parts, CoreRadiusWeights *out)
{
    const SimRational minus = core_radius_number(-1ll, 1ll);
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    CoreRadiusWeights slope;
    CoreRadiusWeights slope_d;
    core_tangent_slope_at(g, mu, scale, parts, &slope);
    core_radius_d(&slope, scale, minus, &slope_d);
    core_radius_l(&slope_d, scale, minus, out);
    core_radius_released(&slope);
    core_radius_released(&slope_d);
    if (parts->plain)
    {
        const SimRational eta_turn = core_radius_times(minus, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j)));
        CoreRadiusWeights first;
        CoreRadiusWeights second;
        CoreRadiusWeights third;
        core_radius_l(g, scale, minus, &first);
        core_radius_eta(&first, &second);
        core_radius_scaled(&second, core_radius_times(eta_turn, scale->cut), &third);
        core_tangent_sum_into(out, &third);
        core_radius_released(&first);
        core_radius_released(&second);
        const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
        core_radius_d(g, scale, minus, &first);
        core_radius_eta(&first, &second);
        core_radius_scaled(&second, core_radius_times(eight_h_j, scale->cut), &third);
        core_tangent_sum_into(out, &third);
        core_radius_released(&first);
        core_radius_released(&second);
    }
}

// the product of `left` and `right`, times `factor` where `scaled` is 1, which the call adds into `into`
static void core_tangent_product_into(CoreRadiusWeights *into, const CoreRadiusWeights *left,
                                      const CoreRadiusWeights *right, int scaled, SimRational factor)
{
    CoreRadiusWeights product;
    core_radius_product(left, right, &product);
    if (scaled)
    {
        CoreRadiusWeights product_scaled;
        core_radius_scaled(&product, factor, &product_scaled);
        core_radius_released(&product);
        core_tangent_sum_into(into, &product_scaled);
    }
    else
    {
        core_tangent_sum_into(into, &product);
    }
}

// `weights` times L, then eta, then `factor`: eta L g at the cut's scale
static void core_tangent_eta_l(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational factor,
                               CoreRadiusWeights *out)
{
    CoreRadiusWeights l;
    CoreRadiusWeights eta;
    core_radius_l(weights, scale, core_radius_number(-1ll, 1ll), &l);
    core_radius_eta(&l, &eta);
    core_radius_scaled(&eta, factor, out);
    core_radius_released(&l);
    core_radius_released(&eta);
}

// `weights` through `first` and then times `factor`, summed into `into`: first is L or eta^2
static void core_tangent_plain_into(CoreRadiusWeights *into, const CoreRadiusWeights *weights,
                                    const CoreRadiusScale *scale, int square, SimRational factor)
{
    CoreRadiusWeights first;
    CoreRadiusWeights scaled;
    if (square)
    {
        core_radius_eta_square(weights, scale, &first);
    }
    else
    {
        core_radius_l(weights, scale, core_radius_number(-1ll, 1ll), &first);
    }
    core_radius_scaled(&first, factor, &scaled);
    core_radius_released(&first);
    core_tangent_sum_into(into, &scaled);
}

// the tangent's axial and swirl numerators at the point, order by order to `last`, for mu fixed, with the parts given
static void core_tangent_full_at(const CoreRadiusOrders *exact, unsigned int last, SimRational mu, const CoreRadiusScale *scale, SimRational xi,
                                 const CoreTangentParts *parts, CoreRadiusSequence *axial_at, CoreRadiusSequence *swirl_at)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational minus = core_radius_number(-1ll, 1ll);
    const CoreRadiusWeights *const f = exact->f;
    const CoreRadiusWeights *const u = exact->g;
    const CoreRadiusWeights *const w = exact->inflow;
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_times(two, scale->a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale->a);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), scale->h);
    // the base's Z parts and the tangent's orders to `last`, each a cell of the temporary tier at last + 1 entries,
    // given back before the call returns
    enum
    {
        ANGULAR_TURNED,
        ALONG_TURNED,
        DF,
        DU,
        DP,
        DW,
        DF_TURNED,
        DU_TURNED,
        FIELDS
    };
    CoreRadiusWeights *field[FIELDS];
    int taken = 1;
    for (unsigned int each = 0u; each < FIELDS; each += 1u)
    {
        field[each] = (CoreRadiusWeights *)platform_cell_cleared(
            (unsigned long long)last + 1ull, sizeof(CoreRadiusWeights), platform_place_g_program_owner, 1);
        taken = taken && (field[each] != NULL);
    }
    CoreRadiusWeights *const angular_turned = field[ANGULAR_TURNED];
    CoreRadiusWeights *const along_turned = field[ALONG_TURNED];
    CoreRadiusWeights *const df = field[DF];
    CoreRadiusWeights *const du = field[DU];
    CoreRadiusWeights *const dp = field[DP];
    CoreRadiusWeights *const dw = field[DW];
    CoreRadiusWeights *const df_turned = field[DF_TURNED];
    CoreRadiusWeights *const du_turned = field[DU_TURNED];
    for (unsigned int k = 0u; taken && (k <= last); k += 1u)
    {
        core_radius_turned_weights(&f[k], angular_c, k, scale, minus, &angular_turned[k]);
        core_radius_turned_weights(&u[k], along_c, k, scale, minus, &along_turned[k]);
    }
    if (taken)
    {
        // df_0 = du_0 = 0, dp_0 = 1
        core_radius_taken(&dp[0], 1u, 1);
        core_radius_pushed(&dp[0], one);
    }
    for (unsigned int k = 0u; taken && (k < last); k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        core_tangent_turned_at(&du[k], along_c, k, mu, scale, parts, &du_turned[k]);
        core_tangent_turned_at(&df[k], angular_c, k, mu, scale, parts, &df_turned[k]);
        core_radius_scaled(&du_turned[k], core_radius_times(minus, sim_rational_reciprocal(next)), &dw[k]);
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale->d);
        const SimRational eta_d = core_radius_times(scale->cut, scale->d);
        CoreRadiusWeights carried;
        CoreRadiusWeights spin;
        CoreRadiusWeights along;
        core_tangent_slope_at(&df[k], mu, scale, parts, &carried);
        core_tangent_eta_l(&carried, scale, eta_d, &spin);
        core_radius_released(&carried);
        core_tangent_slope_at(&du[k], mu, scale, parts, &carried);
        core_tangent_eta_l(&carried, scale, eta_d, &along);
        core_radius_released(&carried);
        if (parts->plain)
        {
            core_tangent_plain_into(&spin, &df[k], scale, 0, core_radius_plus(next, scale->h));
            core_tangent_plain_into(&spin, &df[k], scale, 1, d_eight);
            core_tangent_plain_into(&along, &du[k], scale, 0, core_radius_plus(order_k, scale->a));
            core_tangent_plain_into(&along, &du[k], scale, 1, d_eight);
        }
        if ((k == 0u) || parts->pressure)
        {
            CoreRadiusWeights pressure;
            core_tangent_turned_at(&dp[k], pressure_c, k, mu, scale, parts, &pressure);
            core_tangent_sum_into(&along, &pressure);
        }
        CoreRadiusWeights square = CORE_TANGENT_NONE;
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            const SimRational rest = core_radius_number((long long)j, 1ll);
            core_tangent_product_into(&spin, &f[j], &dw[i], 1, core_radius_plus(rest, one));
            core_tangent_product_into(&spin, &u[i], &df_turned[j], 0, one);
            core_tangent_product_into(&along, &u[j], &dw[i], 1, rest);
            core_tangent_product_into(&along, &u[i], &du_turned[j], 0, one);
            if (parts->plain)
            {
                core_tangent_product_into(&spin, &w[i], &df[j], 1, core_radius_plus(rest, one));
                core_tangent_product_into(&spin, &angular_turned[j], &du[i], 0, one);
                core_tangent_product_into(&along, &w[i], &du[j], 1, rest);
                core_tangent_product_into(&along, &along_turned[j], &du[i], 0, one);
            }
            core_tangent_product_into(&square, &f[i], &df[j], 1, two);
        }
        core_radius_scaled(
            &spin,
            sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))),
            &df[k + 1u]);
        core_radius_scaled(&along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next))),
                           &du[k + 1u]);
        CoreRadiusWeights square_l;
        CoreRadiusWeights square_ll;
        core_radius_l(&square, scale, minus, &square_l);
        core_radius_l(&square_l, scale, minus, &square_ll);
        core_radius_scaled(&square_ll, sim_rational_reciprocal(next), &dp[k + 1u]);
        core_radius_released(&square_l);
        core_radius_released(&square_ll);
        core_radius_released(&spin);
        core_radius_released(&along);
        core_radius_released(&square);
    }
    core_radius_released(axial_at);
    core_radius_released(swirl_at);
    core_radius_taken(axial_at, last + 1u, 1);
    core_radius_taken(swirl_at, last + 1u, 1);
    for (unsigned int k = 0u; taken && (k <= last); k += 1u)
    {
        core_radius_pushed(axial_at, core_radius_value(&du[k], xi));
        core_radius_pushed(swirl_at, core_radius_value(&df[k], xi));
    }
    for (unsigned int each = 0u; each < FIELDS; each += 1u)
    {
        for (unsigned int k = 0u; (field[each] != NULL) && (k <= last); k += 1u)
        {
            core_radius_released(&field[each][k]);
        }
        platform_cell_released(field[each], platform_place_g_program_owner);
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
    // y and g to last + 1 entries go to the caller, and w, to last, the call gives back
    CoreRadiusSequence y = CORE_TANGENT_NONE;
    CoreRadiusSequence g = CORE_TANGENT_NONE;
    CoreRadiusSequence w = CORE_TANGENT_NONE;
    core_radius_taken(&y, last + 1u, 1);
    core_radius_taken(&g, last + 1u, 1);
    core_radius_taken(&w, last, 1);
    core_radius_pushed(&y, core_radius_number(0ll, 1ll));
    core_radius_pushed(&g, core_radius_number(0ll, 1ll));
    for (unsigned int k = 0u; (y.count != 0u) && (k < last); k += 1u)
    {
        const SimRational next = core_radius_number((long long)k + 1ll, 1ll);
        core_radius_pushed(&w, core_radius_over(core_radius_times(core_radius_times(core_radius_number(-1ll, 1ll),
                                                                                    core_radius_times(mu, ld)),
                                                                  y.value[k]),
                                                next));
        SimRational along = core_radius_times(transport, y.value[k]);
        SimRational spin = core_radius_times(transport, g.value[k]);
        SimRational along_sum = core_radius_number(0ll, 1ll);
        SimRational spin_sum = core_radius_number(0ll, 1ll);
        SimRational inflow_along = core_radius_number(0ll, 1ll);
        SimRational inflow_spin = core_radius_number(0ll, 1ll);
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            along_sum = core_radius_plus(along_sum, core_radius_times(u.value[i], y.value[j]));
            spin_sum = core_radius_plus(spin_sum, core_radius_times(u.value[i], g.value[j]));
            inflow_along = core_radius_plus(
                inflow_along,
                core_radius_times(core_radius_times(core_radius_number((long long)j, 1ll), w.value[i]), u.value[j]));
            inflow_spin = core_radius_plus(
                inflow_spin,
                core_radius_times(core_radius_times(core_radius_number((long long)j + 1ll, 1ll), w.value[i]),
                                  f.value[j]));
        }
        along = core_radius_times(mu, core_radius_plus(along, core_radius_times(ld, along_sum)));
        spin = core_radius_times(mu, core_radius_plus(spin, core_radius_times(ld, spin_sum)));
        if (k == 0u)
        {
            along = core_radius_plus(along, core_radius_times(mu, ld));
        }
        along = core_radius_plus(along, inflow_along);
        spin = core_radius_plus(spin, inflow_spin);
        core_radius_pushed(&y, core_radius_over(along, core_radius_times(two, core_radius_times(next, next))));
        core_radius_pushed(
            &g, core_radius_over(spin, core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))));
    }
    core_radius_released(&w);
    core_radius_replaced(axial_at, &y);
    core_radius_replaced(swirl_at, &g);
}

// sum c_k t^k
static SimRational core_tangent_sum_at(const CoreRadiusSequence &c, SimRational t)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    for (unsigned int k = c.count; k > 0u; k -= 1u)
    {
        sum = core_radius_plus(core_radius_times(sum, t), c.value[k - 1u]);
    }
    return sum;
}

// a series made in `made`, of room for reach + 1 coefficients, put in `out`, which may be one of its operands
static void core_tangent_series_made(CoreRadiusSequence *out, CoreRadiusSequence *made)
{
    core_radius_replaced(out, made);
}

// sqrt(series / series_0) = sum e_n Y^n, e_0 = 1: 2 e_n = series_n / series_0 - sum_(i=1..n-1) e_i e_(n-i)
static void core_tangent_root(const CoreRadiusSequence &series, unsigned int reach, CoreRadiusSequence *out)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusSequence root = CORE_TANGENT_NONE;
    core_radius_taken(&root, reach + 1u, 1);
    core_radius_pushed(&root, core_radius_number(1ll, 1ll));
    for (unsigned int power = 1u; (root.count != 0u) && (power <= reach); power += 1u)
    {
        SimRational inner = core_radius_over(series.value[power], series.value[0]);
        for (unsigned int first = 1u; first < power; first += 1u)
        {
            inner = sim_rational_difference(inner, core_radius_times(root.value[first], root.value[power - first]));
        }
        core_radius_pushed(&root, core_radius_times(half, inner));
    }
    core_tangent_series_made(out, &root);
}

// top / bottom as a series, bottom_0 = 1: q_n = top_n - sum_(i=1..n) bottom_i q_(n-i)
static void core_tangent_quotient(const CoreRadiusSequence &top, const CoreRadiusSequence &bottom, unsigned int reach,
                                  CoreRadiusSequence *out)
{
    CoreRadiusSequence quotient = CORE_TANGENT_NONE;
    core_radius_taken(&quotient, reach + 1u, 1);
    for (unsigned int power = 0u; (quotient.room != 0u) && (power <= reach); power += 1u)
    {
        SimRational inner = top.value[power];
        for (unsigned int first = 1u; first <= power; first += 1u)
        {
            inner =
                sim_rational_difference(inner, core_radius_times(bottom.value[first], quotient.value[power - first]));
        }
        core_radius_pushed(&quotient, inner);
    }
    core_tangent_series_made(out, &quotient);
}

// the coefficient of Y^power in (sum left_n Y^n) (sum right_n Y^n)
static SimRational core_tangent_cauchy(const CoreRadiusSequence &left, const CoreRadiusSequence &right, unsigned int power)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    for (unsigned int first = 0u; first <= power; first += 1u)
    {
        sum = core_radius_plus(sum, core_radius_times(left.value[first], right.value[power - first]));
    }
    return sum;
}

// (sum left_n Y^n) (sum right_n Y^n) to Y^reach
static void core_tangent_series_product(const CoreRadiusSequence &left, const CoreRadiusSequence &right,
                                        unsigned int reach, CoreRadiusSequence *out)
{
    CoreRadiusSequence product = CORE_TANGENT_NONE;
    core_radius_taken(&product, reach + 1u, 1);
    for (unsigned int power = 0u; (product.room != 0u) && (power <= reach); power += 1u)
    {
        core_radius_pushed(&product, core_tangent_cauchy(left, right, power));
    }
    core_tangent_series_made(out, &product);
}

// d/dY of sum series_n Y^n to Y^reach, series known to Y^(reach+1)
static void core_tangent_series_derivative(const CoreRadiusSequence &series, unsigned int reach,
                                           CoreRadiusSequence *out)
{
    CoreRadiusSequence derivative = CORE_TANGENT_NONE;
    core_radius_taken(&derivative, reach + 1u, 1);
    for (unsigned int power = 0u; (derivative.room != 0u) && (power <= reach); power += 1u)
    {
        // the power is at max the cfg's order, far inside a long long
        core_radius_pushed(
            &derivative, core_radius_times(core_radius_number((long long)power + 1ll, 1ll), series.value[power + 1u]));
    }
    core_tangent_series_made(out, &derivative);
}

// factor times each coefficient
static void core_tangent_series_scaled(const CoreRadiusSequence &series, SimRational factor, CoreRadiusSequence *out)
{
    CoreRadiusSequence scaled = CORE_TANGENT_NONE;
    core_radius_taken(&scaled, series.count, 1);
    for (unsigned int power = 0u; power < scaled.room; power += 1u)
    {
        core_radius_pushed(&scaled, core_radius_times(factor, series.value[power]));
    }
    core_tangent_series_made(out, &scaled);
}

// left + right coefficient by coefficient, a missing coefficient 0
static void core_tangent_series_sum(const CoreRadiusSequence &left, const CoreRadiusSequence &right,
                                    CoreRadiusSequence *out)
{
    CoreRadiusSequence sum = CORE_TANGENT_NONE;
    core_radius_taken(&sum, (left.count > right.count) ? left.count : right.count, 1);
    for (unsigned int power = 0u; power < sum.room; power += 1u)
    {
        const SimRational one = (power < left.count) ? left.value[power] : core_radius_number(0ll, 1ll);
        core_radius_pushed(&sum, (power < right.count) ? core_radius_plus(one, right.value[power]) : one);
    }
    core_tangent_series_made(out, &sum);
}

// e^(sum series_n Y^n) to Y^reach, series_0 = 0: (n + 1) x_(n+1) = sum_(i=0..n) (i + 1) series_(i+1) x_(n-i)
static void core_tangent_series_exponential(const CoreRadiusSequence &series, unsigned int reach,
                                            CoreRadiusSequence *out)
{
    CoreRadiusSequence exponential = CORE_TANGENT_NONE;
    core_radius_taken(&exponential, reach + 1u, 1);
    core_radius_pushed(&exponential, core_radius_number(1ll, 1ll));
    for (unsigned int power = 0u; (exponential.count != 0u) && (power < reach); power += 1u)
    {
        SimRational sum = core_radius_number(0ll, 1ll);
        for (unsigned int first = 0u; first <= power; first += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            const SimRational weight =
                core_radius_times(core_radius_number((long long)first + 1ll, 1ll), series.value[first + 1u]);
            sum = core_radius_plus(sum, core_radius_times(weight, exponential.value[power - first]));
        }
        // the power is at max the cfg's order, far inside a long long
        core_radius_pushed(&exponential, core_radius_over(sum, core_radius_number((long long)power + 1ll, 1ll)));
    }
    core_tangent_series_made(out, &exponential);
}

// series / s to Y^reach, s = s_0 E^2 with E the root, E_0 = 1
static void core_tangent_over_transport(const CoreRadiusSequence &series, const CoreRadiusSequence &root,
                                        SimRational transport_zero, unsigned int reach, CoreRadiusSequence *out)
{
    CoreRadiusSequence scaled = CORE_TANGENT_NONE;
    core_tangent_series_scaled(series, sim_rational_reciprocal(transport_zero), &scaled);
    core_tangent_quotient(scaled, root, reach, &scaled);
    core_tangent_quotient(scaled, root, reach, &scaled);
    core_tangent_series_made(out, &scaled);
}

// the line "mu <mu>" of the record
static void core_tangent_mu_recorded(FILE *record, SimRational mu)
{
    PlatformText line;
    record_line_begun(&line);
    mmgr_line_put(&line.line, "mu ");
    record_rational(&line.line, mu);
    record_line_ended(record, &line);
}

// the line "  <power> <coefficient>" of the record
static void core_tangent_power_recorded(FILE *record, unsigned int power, SimRational coefficient)
{
    PlatformText line;
    record_line_begun(&line);
    mmgr_line_put(&line.line, "  ");
    mmgr_line_decimal(&line.line, power, 1u);
    mmgr_line_ch(&line.line, ' ');
    record_rational(&line.line, coefficient);
    record_line_ended(record, &line);
}

// the cells of `count` sequences given back
static void core_tangent_sequences_released(CoreRadiusSequence *const *sequences, unsigned int count)
{
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        core_radius_released(sequences[each]);
    }
}

// `u_v` and `f_v`, the base's along and spin read at `xi_v` to `reach`, each a cell of the temporary tier
static void core_tangent_base_at(const CoreRadiusOrders *base, unsigned int reach, SimRational xi_v,
                                 CoreRadiusSequence *u_v, CoreRadiusSequence *f_v)
{
    core_radius_taken(u_v, reach + 1u, 1);
    core_radius_taken(f_v, reach + 1u, 1);
    for (unsigned int k = 0u; k <= reach; k += 1u)
    {
        core_radius_pushed(u_v, core_radius_value(&base->g[k], xi_v));
        core_radius_pushed(f_v, core_radius_value(&base->f[k], xi_v));
    }
}

// `out` taken at `room` on the temporary tier, its first coefficient `first`
static void core_tangent_series_begun(CoreRadiusSequence *out, unsigned int room, SimRational first)
{
    core_radius_taken(out, room, 1);
    core_radius_pushed(out, first);
}

int program_main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    SimRational cut;
    SimRational eta_point;
    // the cfg's lists, each in a cell of the cfg's (run_cfg_rationals)
    CoreRadiusSequence angular = CORE_TANGENT_NONE;
    CoreRadiusSequence axial = CORE_TANGENT_NONE;
    CoreRadiusSequence pressure = CORE_TANGENT_NONE;
    unsigned long long order = 0ull;
    const int read = (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
                     run_cfg_rational(&cfg, "core.anisotropy", &h) &&
                     run_cfg_rationals(&cfg, "core.axis.angular", &angular.value, &angular.count) &&
                     run_cfg_rationals(&cfg, "core.axis.axial", &axial.value, &axial.count) &&
                     run_cfg_rationals(&cfg, "core.axis.pressure", &pressure.value, &pressure.count) &&
                     run_cfg_rational(&cfg, "ellipse.cut", &cut) && run_cfg_count(&cfg, "tangent.order", &order) &&
                     (order >= 2ull) && run_cfg_rational(&cfg, "tangent.eta", &eta_point);
    angular.room = angular.count;
    axial.room = axial.count;
    pressure.room = pressure.count;
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
    core_radius_weights(&scale, &angular, &axial, &pressure, last, minus, &exact);
    const CoreRadiusWeights *const f = exact.f;
    const CoreRadiusWeights *const u = exact.g;
    const CoreRadiusWeights *const w = exact.inflow;
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, h));
    const SimRational along_c = core_radius_times(two, scale.a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale.a);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), h);
    // the base's Z parts and the tangent's orders to `last`, each a cell of the temporary tier at last + 1 entries
    CoreRadiusWeights *const turned = (CoreRadiusWeights *)platform_cell_cleared(
        2ull * ((unsigned long long)last + 1ull), sizeof(CoreRadiusWeights), platform_place_g_program_owner, 1);
    CoreRadiusWeights *const angular_turned = turned;
    CoreRadiusWeights *const along_turned = (turned != NULL) ? (turned + last + 1u) : NULL;
    CoreTangentPowers *const powers = (CoreTangentPowers *)platform_cell_cleared(
        6ull * ((unsigned long long)last + 1ull), sizeof(CoreTangentPowers), platform_place_g_program_owner, 1);
    const int taken = (turned != NULL) && (powers != NULL) && (exact.count > last);
    CoreTangentPowers *const df = powers;
    CoreTangentPowers *const du = taken ? (powers + (last + 1u)) : NULL;
    CoreTangentPowers *const dp = taken ? (powers + 2u * (last + 1u)) : NULL;
    CoreTangentPowers *const dw = taken ? (powers + 3u * (last + 1u)) : NULL;
    CoreTangentPowers *const df_turned = taken ? (powers + 4u * (last + 1u)) : NULL;
    CoreTangentPowers *const du_turned = taken ? (powers + 5u * (last + 1u)) : NULL;
    for (unsigned int k = 0u; taken && (k <= last); k += 1u)
    {
        core_radius_turned_weights(&f[k], angular_c, k, &scale, minus, &angular_turned[k]);
        core_radius_turned_weights(&u[k], along_c, k, &scale, minus, &along_turned[k]);
    }
    // the tangent along e^(mu eta): df_0 = du_0 = 0, dp_0 = 1 at mu^0
    if (taken)
    {
        core_tangent_powers_taken(&dp[0], 1u);
        core_radius_taken(&dp[0].power[0], 1u, 1);
        core_radius_pushed(&dp[0].power[0], one);
    }
    for (unsigned int k = 0u; taken && (k < last); k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        core_tangent_turned(&du[k], along_c, k, &scale, &du_turned[k]);
        core_tangent_turned(&df[k], angular_c, k, &scale, &df_turned[k]);
        core_tangent_scaled(&du_turned[k], core_radius_times(minus, sim_rational_reciprocal(next)), &dw[k]);
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale.d);
        CoreTangentPowers part = {NULL, 0u};
        CoreTangentPowers other = {NULL, 0u};
        CoreTangentPowers spin = {NULL, 0u};
        core_tangent_l(&df[k], &scale, &part);
        core_tangent_scaled(&part, core_radius_plus(next, h), &spin);
        core_tangent_slope(&df[k], &scale, &part);
        core_tangent_l(&part, &scale, &part);
        core_tangent_eta(&part, &scale, &part);
        core_tangent_scaled(&part, scale.d, &part);
        core_tangent_sum(&spin, &part, &spin);
        core_tangent_eta_square(&df[k], &scale, &part);
        core_tangent_scaled(&part, d_eight, &part);
        core_tangent_sum(&spin, &part, &spin);
        CoreTangentPowers along = {NULL, 0u};
        core_tangent_l(&du[k], &scale, &part);
        core_tangent_scaled(&part, core_radius_plus(order_k, scale.a), &along);
        core_tangent_slope(&du[k], &scale, &part);
        core_tangent_l(&part, &scale, &part);
        core_tangent_eta(&part, &scale, &part);
        core_tangent_scaled(&part, scale.d, &part);
        core_tangent_sum(&along, &part, &along);
        core_tangent_eta_square(&du[k], &scale, &part);
        core_tangent_scaled(&part, d_eight, &part);
        core_tangent_sum(&along, &part, &along);
        core_tangent_turned(&dp[k], pressure_c, k, &scale, &part);
        core_tangent_sum(&along, &part, &along);
        CoreTangentPowers square = {NULL, 0u};
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const unsigned int j = k - i;
            const SimRational rest = core_radius_number((long long)j, 1ll);
            core_tangent_product(&f[j], &dw[i], &part);
            core_tangent_product(&w[i], &df[j], &other);
            core_tangent_sum(&part, &other, &part);
            core_tangent_scaled(&part, core_radius_plus(rest, one), &part);
            core_tangent_sum(&spin, &part, &spin);
            core_tangent_product(&angular_turned[j], &du[i], &part);
            core_tangent_product(&u[i], &df_turned[j], &other);
            core_tangent_sum(&part, &other, &part);
            core_tangent_sum(&spin, &part, &spin);
            core_tangent_product(&u[j], &dw[i], &part);
            core_tangent_product(&w[i], &du[j], &other);
            core_tangent_sum(&part, &other, &part);
            core_tangent_scaled(&part, rest, &part);
            core_tangent_sum(&along, &part, &along);
            core_tangent_product(&along_turned[j], &du[i], &part);
            core_tangent_product(&u[i], &du_turned[j], &other);
            core_tangent_sum(&part, &other, &part);
            core_tangent_sum(&along, &part, &along);
            core_tangent_product(&f[i], &df[j], &part);
            core_tangent_scaled(&part, two, &part);
            core_tangent_sum(&square, &part, &square);
        }
        core_tangent_scaled(
            &spin,
            sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))),
            &df[k + 1u]);
        core_tangent_scaled(&along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next))),
                            &du[k + 1u]);
        core_tangent_l(&square, &scale, &part);
        core_tangent_l(&part, &scale, &part);
        core_tangent_scaled(&part, sim_rational_reciprocal(next), &dp[k + 1u]);
        core_tangent_powers_released(&part);
        core_tangent_powers_released(&other);
        core_tangent_powers_released(&spin);
        core_tangent_powers_released(&along);
        core_tangent_powers_released(&square);
    }
    // 1 and 2: the degrees, and the top weights against the products of sigma L
    const CoreRadiusWeights f0 = f[0];
    const CoreRadiusWeights u0 = u[0];
    CoreRadiusWeights unit_weights = CORE_TANGENT_NONE;
    core_radius_taken(&unit_weights, 1u, 1);
    core_radius_pushed(&unit_weights, one);
    CoreRadiusWeights top_u = CORE_TANGENT_NONE;
    CoreRadiusWeights top_f = CORE_TANGENT_NONE;
    CoreRadiusWeights first = CORE_TANGENT_NONE;
    CoreRadiusWeights second = CORE_TANGENT_NONE;
    core_radius_d(&unit_weights, &scale, minus, &first);
    core_radius_l(&first, &scale, minus, &second);
    core_radius_scaled(&second, half, &top_u);
    core_radius_released(&first);
    core_radius_released(&second);
    core_radius_released(&unit_weights);
    int degrees = taken;
    int tops = taken;
    for (unsigned int k = 1u; taken && (k <= last); k += 1u)
    {
        degrees = degrees && (core_tangent_degree(&du[k]) == k) && ((k < 2u) || (core_tangent_degree(&df[k]) == k));
        tops = tops && (du[k].count > k) && core_radius_same(&du[k].power[k], &top_u) &&
               ((k < 2u) || ((df[k].count > k) && core_radius_same(&df[k].power[k], &top_f)));
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        // sigma L g = D eta L g + U_0 L d g
        CoreRadiusWeights sigma_u = CORE_TANGENT_NONE;
        CoreRadiusWeights sigma_f = CORE_TANGENT_NONE;
        CoreRadiusWeights *const tops_now[2] = {&top_u, &top_f};
        CoreRadiusWeights *const sigmas[2] = {&sigma_u, &sigma_f};
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            CoreRadiusWeights l = CORE_TANGENT_NONE;
            CoreRadiusWeights eta = CORE_TANGENT_NONE;
            CoreRadiusWeights eta_cut = CORE_TANGENT_NONE;
            CoreRadiusWeights d = CORE_TANGENT_NONE;
            CoreRadiusWeights ld = CORE_TANGENT_NONE;
            CoreRadiusWeights carried = CORE_TANGENT_NONE;
            core_radius_l(tops_now[which], &scale, minus, &l);
            core_radius_eta(&l, &eta);
            core_radius_scaled(&eta, cut, &eta_cut);
            core_radius_scaled(&eta_cut, scale.d, sigmas[which]);
            core_radius_d(tops_now[which], &scale, minus, &d);
            core_radius_l(&d, &scale, minus, &ld);
            core_radius_product(&u0, &ld, &carried);
            core_tangent_sum_into(sigmas[which], &carried);
            CoreRadiusWeights *const done[5] = {&l, &eta, &eta_cut, &d, &ld};
            for (unsigned int each = 0u; each < 5u; each += 1u)
            {
                core_radius_released(done[each]);
            }
        }
        CoreRadiusWeights top_w = CORE_TANGENT_NONE;
        core_radius_d(&top_u, &scale, minus, &first);
        core_radius_l(&first, &scale, minus, &second);
        core_radius_scaled(&second, core_radius_times(minus, sim_rational_reciprocal(next)), &top_w);
        core_radius_released(&first);
        core_radius_released(&second);
        core_radius_product(&f0, &top_w, &first);
        core_radius_sum(&sigma_f, &first, &second);
        core_radius_released(&first);
        core_radius_released(&top_f);
        core_radius_scaled(
            &second,
            sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))),
            &top_f);
        core_radius_released(&second);
        core_radius_released(&top_u);
        core_radius_scaled(&sigma_u, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next))),
                           &top_u);
        core_radius_released(&sigma_u);
        core_radius_released(&sigma_f);
        core_radius_released(&top_w);
    }
    core_radius_released(&top_u);
    core_radius_released(&top_f);
    sim_check(&results, degrees, "du_k has degree k in mu from k = 1 and df_k from k = 2");
    sim_check(&results, tops, "the top weights of du_k and df_k are the products of sigma L at every order");
    // 3: at the cfg's eta, sigma, d and the sign of the top coefficient of dF_k = df_k / L^(2k)
    const SimRational eta_square = core_radius_times(eta_point, eta_point);
    const SimRational l_point = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_square));
    const SimRational d_point = sim_rational_difference(one, eta_square);
    const SimRational u0_point = core_radius_value(&u0, xi);
    const SimRational sigma_point = core_radius_plus(core_radius_times(scale.d, eta_point), core_radius_times(u0_point, d_point));
    int sign = 0;
    int one_sign = taken && (sim_rational_sign(sigma_point) > 0) && (sim_rational_sign(d_point) > 0) &&
                   (sim_rational_sign(l_point) > 0);
    if (record != NULL)
    {
        PlatformText line;
        record_line_begun(&line);
        mmgr_line_put(&line.line, "tangent along e^(mu eta) at eta ");
        record_rational(&line.line, eta_point);
        mmgr_line_put(&line.line, ", xi ");
        record_rational(&line.line, xi);
        mmgr_line_put(&line.line, ", sigma ");
        record_rational(&line.line, sigma_point);
        mmgr_line_put(&line.line, ": for each k, the coefficients of mu^0 to mu^k of dF_k e^(-mu eta)");
        record_line_ended(record, &line);
    }
    SimRational l_power = one;
    for (unsigned int k = 1u; taken && (k <= last); k += 1u)
    {
        l_power = core_radius_times(l_power, core_radius_times(l_point, l_point));
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "  ");
            mmgr_line_decimal(&line.line, k, 1u);
            for (unsigned int n = 0u; n < df[k].count; n += 1u)
            {
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line, core_radius_over(core_radius_value(&df[k].power[n], xi), l_power));
            }
            record_line_ended(record, &line);
        }
        if (k >= 2u)
        {
            const int top_sign = sim_rational_sign(core_radius_value(&df[k].power[k], xi));
            one_sign = one_sign && (top_sign != 0) && ((sign == 0) || (top_sign == sign));
            sign = top_sign;
        }
    }
    sim_check(&results, one_sign, "sigma, d and L past 0 at the eta, and the top coefficient of dF_k of one sign at every order");
    mmgr_line_put(&results.line, "  sigma at the eta: ");
    sim_rational_print(&results.line, sigma_point);
    mmgr_line_put(&results.line, (sign < 0) ? ", the top coefficients below 0\n" : ", the top coefficients past 0\n");
    sim_flush(&results);
    for (unsigned int k = 0u; (powers != NULL) && (k < 6u * (last + 1u)); k += 1u)
    {
        core_tangent_powers_released(&powers[k]);
    }
    for (unsigned int k = 0u; (turned != NULL) && (k < 2u * (last + 1u)); k += 1u)
    {
        core_radius_released(&turned[k]);
    }
    platform_cell_released(powers, platform_place_g_program_owner);
    platform_cell_released(turned, platform_place_g_program_owner);
    // the measurement at the vertex, where the cfg contains it
    SimRational measure_rho;
    SimRational measure_step;
    unsigned long long measure_order = 0ull;
    unsigned long long measure_count = 0ull;
    CoreRadiusSequence measure_mu = CORE_TANGENT_NONE;
    const int measure = run_cfg_rational(&cfg, "measure.rho", &measure_rho) &&
                        (sim_rational_sign(sim_rational_difference(measure_rho, one)) > 0) &&
                        run_cfg_count(&cfg, "measure.order", &measure_order) && (measure_order >= 2ull) &&
                        run_cfg_rationals(&cfg, "measure.mu", &measure_mu.value, &measure_mu.count) &&
                        run_cfg_rational(&cfg, "measure.step", &measure_step) &&
                        run_cfg_count(&cfg, "measure.count", &measure_count);
    measure_mu.room = measure_mu.count;
    if (measure)
    {
        const unsigned int reach = (unsigned int)measure_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, &angular, &axial, &pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(measure_rho, sim_rational_reciprocal(measure_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v = CORE_TANGENT_NONE;
        CoreRadiusSequence f_v = CORE_TANGENT_NONE;
        core_tangent_base_at(&base, reach, xi_v, &u_v, &f_v);
        // s = L (D eta + d U), and sqrt(s / s_0) = sum e_k Y^k
        CoreRadiusSequence transport = CORE_TANGENT_NONE;
        core_radius_taken(&transport, reach + 1u, 1);
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            const SimRational inner = core_radius_times(d_v, u_v.value[power]);
            core_radius_pushed(&transport,
                               core_radius_times(l_v, (power == 0u)
                                                          ? core_radius_plus(core_radius_times(scale.d, eta_v), inner)
                                                          : inner));
        }
        CoreRadiusSequence root = CORE_TANGENT_NONE;
        core_tangent_root(transport, reach, &root);
        CoreRadiusSequence phi_series = CORE_TANGENT_NONE;
        core_radius_taken(&phi_series, reach + 1u, 1);
        for (unsigned int k = 0u; k <= reach; k += 1u)
        {
            core_radius_pushed(&phi_series,
                               core_radius_over(root.value[k], core_radius_number(2ll * (long long)k + 1ll, 1ll)));
        }
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "measurement at the vertex eta ");
            record_rational(&line.line, eta_v);
            mmgr_line_put(&line.line, " of E_rho, rho ");
            record_rational(&line.line, measure_rho);
            mmgr_line_put(&line.line, ", to order ");
            mmgr_line_decimal(&line.line, reach, 1u);
            mmgr_line_put(&line.line, ": s_0 ");
            record_rational(&line.line, transport.value[0]);
            mmgr_line_put(&line.line, "; then Y and phi, Phi = sqrt(2 s_0 Y) phi; then for each mu, Y, principal "
                                      "axial, full axial, principal swirl, full swirl");
            record_line_ended(record, &line);
            for (unsigned long long index = 1ull; index <= measure_count; index += 1ull)
            {
                const SimRational y_point = core_radius_times(measure_step, core_radius_number((long long)index, 1ll));
                record_line_begun(&line);
                mmgr_line_put(&line.line, "  ");
                record_rational(&line.line, y_point);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line, core_tangent_sum_at(phi_series, y_point));
                record_line_ended(record, &line);
            }
        }
        for (unsigned int at = 0u; at < measure_mu.count; at += 1u)
        {
            const SimRational mu = measure_mu.value[at];
            CoreRadiusSequence axial_full = CORE_TANGENT_NONE;
            CoreRadiusSequence swirl_full = CORE_TANGENT_NONE;
            const CoreTangentParts every = {1, 1, 1};
            core_tangent_full_at(&base, reach, mu, &scale, xi_v, &every, &axial_full, &swirl_full);
            CoreRadiusSequence axial_principal = CORE_TANGENT_NONE;
            CoreRadiusSequence swirl_principal = CORE_TANGENT_NONE;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            if (record != NULL)
            {
                core_tangent_mu_recorded(record, mu);
                for (unsigned long long index = 1ull; index <= measure_count; index += 1ull)
                {
                    const SimRational y_point = core_radius_times(measure_step, core_radius_number((long long)index, 1ll));
                    PlatformText line;
                    record_line_begun(&line);
                    mmgr_line_put(&line.line, "  ");
                    record_rational(&line.line, y_point);
                    const CoreRadiusSequence *const series[4] = {&axial_principal, &axial_full, &swirl_principal,
                                                                 &swirl_full};
                    for (unsigned int which = 0u; which < 4u; which += 1u)
                    {
                        mmgr_line_ch(&line.line, ' ');
                        record_rational(&line.line, core_tangent_sum_at(*series[which], y_point));
                    }
                    record_line_ended(record, &line);
                }
            }
            CoreRadiusSequence *const done[4] = {&axial_full, &swirl_full, &axial_principal, &swirl_principal};
            core_tangent_sequences_released(done, 4u);
        }
        mmgr_line_put(&results.line, "  measurement at the vertex written to the record\n");
        sim_flush(&results);
        CoreRadiusSequence *const done[5] = {&u_v, &f_v, &transport, &root, &phi_series};
        core_tangent_sequences_released(done, 5u);
        core_radius_orders_released(&base);
    }
    // the remainder taken apart at the vertex, where the cfg contains it: each part the principal system leaves out,
    // alone
    SimRational apart_rho;
    SimRational apart_step;
    unsigned long long apart_order = 0ull;
    unsigned long long apart_count = 0ull;
    CoreRadiusSequence apart_mu = CORE_TANGENT_NONE;
    const int apart = run_cfg_rational(&cfg, "apart.rho", &apart_rho) &&
                      (sim_rational_sign(sim_rational_difference(apart_rho, one)) > 0) &&
                      run_cfg_count(&cfg, "apart.order", &apart_order) && (apart_order >= 2ull) &&
                      run_cfg_rationals(&cfg, "apart.mu", &apart_mu.value, &apart_mu.count) &&
                      run_cfg_rational(&cfg, "apart.step", &apart_step) &&
                      run_cfg_count(&cfg, "apart.count", &apart_count);
    apart_mu.room = apart_mu.count;
    if (apart)
    {
        const unsigned int reach = (unsigned int)apart_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, &angular, &axial, &pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(apart_rho, sim_rational_reciprocal(apart_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v = CORE_TANGENT_NONE;
        CoreRadiusSequence f_v = CORE_TANGENT_NONE;
        core_tangent_base_at(&base, reach, xi_v, &u_v, &f_v);
        // none, the slope alone, the plain terms alone, the pressure alone, and every part
        const CoreTangentParts choices[5] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
        int principal_same = 1;
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "remainder taken apart at the vertex eta ");
            record_rational(&line.line, eta_v);
            mmgr_line_put(&line.line, " of E_rho, rho ");
            record_rational(&line.line, apart_rho);
            mmgr_line_put(&line.line, ", to order ");
            mmgr_line_decimal(&line.line, reach, 1u);
            mmgr_line_put(&line.line, ": for each mu, Y, then axial and swirl each for the principal, the slope alone, "
                                      "the plain terms alone, the pressure alone, and every part");
            record_line_ended(record, &line);
        }
        for (unsigned int at = 0u; at < apart_mu.count; at += 1u)
        {
            const SimRational mu = apart_mu.value[at];
            CoreRadiusSequence axial_parts[5] = {CORE_TANGENT_NONE, CORE_TANGENT_NONE, CORE_TANGENT_NONE,
                                                 CORE_TANGENT_NONE, CORE_TANGENT_NONE};
            CoreRadiusSequence swirl_parts[5] = {CORE_TANGENT_NONE, CORE_TANGENT_NONE, CORE_TANGENT_NONE,
                                                 CORE_TANGENT_NONE, CORE_TANGENT_NONE};
            for (unsigned int choice = 0u; choice < 5u; choice += 1u)
            {
                core_tangent_full_at(&base, reach, mu, &scale, xi_v, &choices[choice], &axial_parts[choice], &swirl_parts[choice]);
            }
            CoreRadiusSequence axial_principal = CORE_TANGENT_NONE;
            CoreRadiusSequence swirl_principal = CORE_TANGENT_NONE;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            for (unsigned int k = 0u; k <= reach; k += 1u)
            {
                principal_same =
                    principal_same && (axial_parts[0].count > k) && (axial_principal.count > k) &&
                    (swirl_parts[0].count > k) && (swirl_principal.count > k) &&
                    (sim_rational_sign(sim_rational_difference(axial_parts[0].value[k], axial_principal.value[k])) ==
                     0) &&
                    (sim_rational_sign(sim_rational_difference(swirl_parts[0].value[k], swirl_principal.value[k])) ==
                     0);
            }
            if (record != NULL)
            {
                core_tangent_mu_recorded(record, mu);
                for (unsigned long long index = 1ull; index <= apart_count; index += 1ull)
                {
                    const SimRational y_point = core_radius_times(apart_step, core_radius_number((long long)index, 1ll));
                    PlatformText line;
                    record_line_begun(&line);
                    mmgr_line_put(&line.line, "  ");
                    record_rational(&line.line, y_point);
                    for (unsigned int choice = 0u; choice < 5u; choice += 1u)
                    {
                        mmgr_line_ch(&line.line, ' ');
                        record_rational(&line.line, core_tangent_sum_at(axial_parts[choice], y_point));
                    }
                    for (unsigned int choice = 0u; choice < 5u; choice += 1u)
                    {
                        mmgr_line_ch(&line.line, ' ');
                        record_rational(&line.line, core_tangent_sum_at(swirl_parts[choice], y_point));
                    }
                    record_line_ended(record, &line);
                }
            }
            for (unsigned int choice = 0u; choice < 5u; choice += 1u)
            {
                core_radius_released(&axial_parts[choice]);
                core_radius_released(&swirl_parts[choice]);
            }
            core_radius_released(&axial_principal);
            core_radius_released(&swirl_principal);
        }
        sim_check(&results, principal_same, "with every part off the tangent's weights read at the vertex are the principal system's, order by order");
        core_radius_released(&u_v);
        core_radius_released(&f_v);
        core_radius_orders_released(&base);
    }
    // the slope's share at larger mu, where the cfg contains it: the principal system at the point and the tangent with
    // the slope alone, each with the size of its last order's term over its sum, which shows how far the order resolves
    // it
    SimRational slope_rho;
    unsigned long long slope_order = 0ull;
    CoreRadiusSequence slope_mu = CORE_TANGENT_NONE;
    CoreRadiusSequence slope_y = CORE_TANGENT_NONE;
    const int slope_share = run_cfg_rational(&cfg, "slope.rho", &slope_rho) &&
                            (sim_rational_sign(sim_rational_difference(slope_rho, one)) > 0) &&
                            run_cfg_count(&cfg, "slope.order", &slope_order) && (slope_order >= 2ull) &&
                            run_cfg_rationals(&cfg, "slope.mu", &slope_mu.value, &slope_mu.count) &&
                            run_cfg_rationals(&cfg, "slope.y", &slope_y.value, &slope_y.count);
    slope_mu.room = slope_mu.count;
    slope_y.room = slope_y.count;
    if (slope_share)
    {
        const unsigned int reach = (unsigned int)slope_order;
        CoreRadiusOrders base;
        core_radius_weights(&scale, &angular, &axial, &pressure, reach, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(slope_rho, sim_rational_reciprocal(slope_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        CoreRadiusSequence u_v = CORE_TANGENT_NONE;
        CoreRadiusSequence f_v = CORE_TANGENT_NONE;
        core_tangent_base_at(&base, reach, xi_v, &u_v, &f_v);
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "slope's share at the vertex eta ");
            record_rational(&line.line, eta_v);
            mmgr_line_put(&line.line, " of E_rho, rho ");
            record_rational(&line.line, slope_rho);
            mmgr_line_put(&line.line, ", to order ");
            mmgr_line_decimal(&line.line, reach, 1u);
            mmgr_line_put(&line.line, ": for each mu, Y, then axial and swirl each as principal, slope alone, and the "
                                      "last order's term over the sum for each");
            record_line_ended(record, &line);
        }
        const CoreTangentParts slope_alone = {1, 0, 0};
        for (unsigned int at = 0u; at < slope_mu.count; at += 1u)
        {
            const SimRational mu = slope_mu.value[at];
            CoreRadiusSequence axial_slope = CORE_TANGENT_NONE;
            CoreRadiusSequence swirl_slope = CORE_TANGENT_NONE;
            core_tangent_full_at(&base, reach, mu, &scale, xi_v, &slope_alone, &axial_slope, &swirl_slope);
            CoreRadiusSequence axial_principal = CORE_TANGENT_NONE;
            CoreRadiusSequence swirl_principal = CORE_TANGENT_NONE;
            core_tangent_principal_at(u_v, f_v, l_v, d_v, eta_v, scale.d, reach, mu, &axial_principal, &swirl_principal);
            if (record != NULL)
            {
                core_tangent_mu_recorded(record, mu);
                for (unsigned int y_at = 0u; y_at < slope_y.count; y_at += 1u)
                {
                    const SimRational y_point = slope_y.value[y_at];
                    SimRational last_power = one;
                    for (unsigned int k = 0u; k < reach; k += 1u)
                    {
                        last_power = core_radius_times(last_power, y_point);
                    }
                    PlatformText line;
                    record_line_begun(&line);
                    mmgr_line_put(&line.line, "  ");
                    record_rational(&line.line, y_point);
                    const CoreRadiusSequence *const series[4] = {&axial_principal, &axial_slope, &swirl_principal, &swirl_slope};
                    for (unsigned int which = 0u; which < 4u; which += 1u)
                    {
                        const SimRational sum = core_tangent_sum_at(*series[which], y_point);
                        const SimRational last_term = core_radius_times(series[which]->value[reach], last_power);
                        mmgr_line_ch(&line.line, ' ');
                        record_rational(&line.line, sum);
                        mmgr_line_ch(&line.line, ' ');
                        record_rational(&line.line, core_radius_over(last_term, sum));
                    }
                    record_line_ended(record, &line);
                }
            }
            CoreRadiusSequence *const done[4] = {&axial_slope, &swirl_slope, &axial_principal, &swirl_principal};
            core_tangent_sequences_released(done, 4u);
        }
        core_radius_released(&u_v);
        core_radius_released(&f_v);
        core_radius_orders_released(&base);
    }
    // the phase in both directions at the vertex, where the cfg contains it. The slope turns mu into mu + d/deta at Y
    // fixed, and with the growth e^(sqrt(mu) Psi(Y, eta)) the principal system's 2 Y Psi_Y^2 = s becomes 2 Y mu Psi_Y^2
    // = s (mu + sqrt(mu) Psi_eta): Psi = Phi + Phi_1 / sqrt(mu) + ..., Phi_1 = (1/2) int_0^Y Phi' Phi_eta dY. With
    // sqrt(s) = sqrt(s_0) sum e_n Y^n and s_eta / sum e_n Y^n = sum g_n Y^n, Phi_eta = sqrt(Y / (2 s_0)) sum g_n Y^n /
    // (2n + 1), and
    //     Phi_1(Y) = (1/4) sum_n Y^(n+1) / (n + 1) sum_(i+j=n) e_i g_j / (2j + 1),
    // every coefficient exact: the slope's share of the principal nears e^(Phi_1(Y)) - 1 as mu grows.
    SimRational phase_rho;
    unsigned long long phase_order = 0ull;
    CoreRadiusSequence phase_y = CORE_TANGENT_NONE;
    const int phase = run_cfg_rational(&cfg, "phase.rho", &phase_rho) &&
                      (sim_rational_sign(sim_rational_difference(phase_rho, one)) > 0) &&
                      run_cfg_count(&cfg, "phase.order", &phase_order) && (phase_order >= 2ull) &&
                      run_cfg_rationals(&cfg, "phase.y", &phase_y.value, &phase_y.count);
    phase_y.room = phase_y.count;
    if (phase)
    {
        // the cfg's order is a count of orders, far inside an unsigned int
        const unsigned int reach = (unsigned int)phase_order;
        // one order past the reach, for the derivatives in Y the amplitude reads
        const unsigned int span = reach + 1u;
        CoreRadiusOrders base;
        core_radius_weights(&scale, &angular, &axial, &pressure, span, minus, &base);
        const SimRational xi_v = core_radius_over(core_radius_plus(phase_rho, sim_rational_reciprocal(phase_rho)), two);
        const SimRational eta_v = core_radius_times(cut, xi_v);
        const SimRational eta_v_square = core_radius_times(eta_v, eta_v);
        const SimRational l_v = sim_rational_difference(one, core_radius_times(core_radius_times(two, h), eta_v_square));
        const SimRational d_v = sim_rational_difference(one, eta_v_square);
        // L_eta = -4 h eta, L_eta_eta = -4 h, d_eta = -2 eta and d_eta_eta = -2, and U_eta = (1 / a) U_xi
        const SimRational l_eta = core_radius_times(core_radius_times(core_radius_number(-4ll, 1ll), h), eta_v);
        const SimRational l_eta_eta = core_radius_times(core_radius_number(-4ll, 1ll), h);
        const SimRational d_eta = core_radius_times(core_radius_number(-2ll, 1ll), eta_v);
        const SimRational d_eta_eta = core_radius_number(-2ll, 1ll);
        // Every series of the phase, each a cell of the temporary tier that the block gives back when it ends. The two
        // `part` series hold a term that an expression reads once
        enum
        {
            ALONG_SERIES,
            ALONG_ETA_SERIES,
            SWIRL_SERIES,
            SWIRL_ETA_SERIES,
            TRANSPORT,
            TRANSPORT_ETA,
            TRANSPORT_ETA_ETA,
            ROOT,
            QUOTIENT,
            PHI_ETA,
            PHI_SLOPE,
            ROOT_ETA_TOP,
            ROOT_ETA,
            QUOTIENT_ETA,
            PHI_ETA_ETA,
            PHI_PRODUCT_ETA,
            TRANSPORT_PRIME,
            TRANSPORT_ETA_PRIME,
            ALONG_PRIME,
            ALONG_ETA_PRIME,
            INFLOW_COEFFICIENT,
            INFLOW_COEFFICIENT_ETA,
            PRIME_PART,
            INFLOW_PART,
            AMPLITUDE_RATE_ETA,
            AMPLITUDE_ETA,
            ROOT_PHI_SQUARE,
            AMPLITUDE_PART,
            AMPLITUDE_SLOPE,
            INFLOW_RATE,
            INFLOW_RATE_ETA,
            INFLOW_EXPONENT,
            INFLOW_EXPONENT_ETA,
            INFLOW_EXPONENTIAL,
            SWIRL_Y_PRIME,
            SWIRL_ETA_Y_PRIME,
            SWIRL_FORCING,
            SWIRL_FORCING_TOP,
            SWIRL_FORCING_ETA,
            SWIRL_CARRIED,
            SWIRL_CARRIED_ETA,
            SWIRL_INTEGRAL,
            SWIRL_INTEGRAL_ETA,
            SWIRL_RATIO,
            SWIRL_RATIO_ETA,
            PRIME_RATE,
            AMPLITUDE_LOG_RATE,
            PHI_PRODUCT,
            PHI_PRODUCT_PRIME,
            SWIRL_MOMENT_PRIME,
            RATIO_GROWTH,
            RATIO_SLOPE,
            RATIO_FORCING,
            SWIRL_SOURCE,
            SOURCE_CARRIED,
            SOURCE_INTEGRAL,
            SWIRL_SLOPE,
            PART,
            OTHER,
            SERIES
        };
        static CoreRadiusSequence s_series[SERIES];
        for (unsigned int each = 0u; each < SERIES; each += 1u)
        {
            s_series[each] = CoreRadiusSequence CORE_TANGENT_NONE;
        }
        CoreRadiusSequence &along_series = s_series[ALONG_SERIES];
        CoreRadiusSequence &along_eta_series = s_series[ALONG_ETA_SERIES];
        CoreRadiusSequence &swirl_series = s_series[SWIRL_SERIES];
        CoreRadiusSequence &swirl_eta_series = s_series[SWIRL_ETA_SERIES];
        CoreRadiusSequence &transport = s_series[TRANSPORT];
        CoreRadiusSequence &transport_eta = s_series[TRANSPORT_ETA];
        CoreRadiusSequence &transport_eta_eta = s_series[TRANSPORT_ETA_ETA];
        CoreRadiusSequence &part = s_series[PART];
        CoreRadiusSequence &other = s_series[OTHER];
        // U and U_eta, F and F_eta, and s, the transport's coefficient, with its first two eta-derivatives, each as a series in Y
        CoreRadiusSequence *const filled[7] = {&along_series, &along_eta_series, &swirl_series,     &swirl_eta_series,
                                               &transport,    &transport_eta,    &transport_eta_eta};
        for (unsigned int each = 0u; each < 7u; each += 1u)
        {
            core_radius_taken(filled[each], span + 1u, 1);
        }
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            CoreRadiusWeights along_xi = CORE_TANGENT_NONE;
            CoreRadiusWeights along_xi_xi = CORE_TANGENT_NONE;
            CoreRadiusWeights swirl_xi = CORE_TANGENT_NONE;
            core_radius_slope(&base.g[power], &along_xi);
            core_radius_slope(&along_xi, &along_xi_xi);
            core_radius_slope(&base.f[power], &swirl_xi);
            const SimRational along = core_radius_value(&base.g[power], xi_v);
            const SimRational along_eta = core_radius_over(core_radius_value(&along_xi, xi_v), cut);
            const SimRational along_eta_eta =
                core_radius_over(core_radius_value(&along_xi_xi, xi_v), core_radius_times(cut, cut));
            SimRational inner = core_radius_times(d_v, along);
            SimRational inner_eta = core_radius_plus(core_radius_times(d_eta, along), core_radius_times(d_v, along_eta));
            const SimRational inner_eta_eta = core_radius_plus(core_radius_plus(core_radius_times(d_eta_eta, along), core_radius_times(core_radius_times(two, d_eta), along_eta)),
                                                               core_radius_times(d_v, along_eta_eta));
            if (power == 0u)
            {
                inner = core_radius_plus(inner, core_radius_times(scale.d, eta_v));
                inner_eta = core_radius_plus(inner_eta, scale.d);
            }
            core_radius_pushed(&along_series, along);
            core_radius_pushed(&along_eta_series, along_eta);
            core_radius_pushed(&swirl_series, core_radius_value(&base.f[power], xi_v));
            core_radius_pushed(&swirl_eta_series, core_radius_over(core_radius_value(&swirl_xi, xi_v), cut));
            core_radius_pushed(&transport, core_radius_times(l_v, inner));
            core_radius_pushed(&transport_eta,
                               core_radius_plus(core_radius_times(l_eta, inner), core_radius_times(l_v, inner_eta)));
            core_radius_pushed(
                &transport_eta_eta,
                core_radius_plus(core_radius_plus(core_radius_times(l_eta_eta, inner),
                                                  core_radius_times(core_radius_times(two, l_eta), inner_eta)),
                                 core_radius_times(l_v, inner_eta_eta)));
            core_radius_released(&along_xi);
            core_radius_released(&along_xi_xi);
            core_radius_released(&swirl_xi);
        }
        CoreRadiusSequence &root = s_series[ROOT];
        CoreRadiusSequence &quotient = s_series[QUOTIENT];
        core_tangent_root(transport, span, &root);
        core_tangent_quotient(transport_eta, root, span, &quotient);
        int multiplied = 1;
        CoreRadiusSequence &phi_eta = s_series[PHI_ETA];
        core_radius_taken(&phi_eta, span + 1u, 1);
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            const SimRational square_gap = sim_rational_difference(
                core_tangent_cauchy(root, root, power), core_radius_over(transport.value[power], transport.value[0]));
            const SimRational product_gap =
                sim_rational_difference(core_tangent_cauchy(root, quotient, power), transport_eta.value[power]);
            multiplied = multiplied && (sim_rational_sign(square_gap) == 0) && (sim_rational_sign(product_gap) == 0);
            // the power is at max the cfg's order, far inside a long long
            core_radius_pushed(&phi_eta, core_radius_over(quotient.value[power],
                                                          core_radius_number(2ll * (long long)power + 1ll, 1ll)));
        }
        sim_check(&results, multiplied, "the root squared is s / s_0 and the root times the quotient is s_eta at every order");
        CoreRadiusSequence &phi_slope = s_series[PHI_SLOPE];
        core_tangent_series_begun(&phi_slope, reach + 1u, core_radius_number(0ll, 1ll));
        for (unsigned int power = 1u; power <= reach; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            core_radius_pushed(&phi_slope, core_radius_over(core_tangent_cauchy(root, phi_eta, power - 1u),
                                                            core_radius_number(4ll * (long long)power, 1ll)));
        }
        // The amplitude past Phi_1. With y = e^(sqrt(mu) Phi) (A_0 + A_1 / sqrt(mu) + ...), the terms of order 1 / mu give
        // 4 Y Phi' (A_1 / A_0)' = R / A_0, and the slope's part of A_1 / A_0 less the principal's is D(Y) = int_0^Y dR / (4 Y Phi') dY,
        //     dR = -P / 2 - Y E (P / E)' - Y P^2 / 2 + s (ln A_0)_eta,   P = Phi' Phi_eta = E H / 2,
        // E = sum e_n Y^n and H = sum g_n Y^n / (2n + 1), the inflow's terms canceling. A_0 starts from the Bessel solution
        // (L d / s_0) (I_0(sqrt(2 s_0 mu Y)) - 1) near Y = 0: A_0 = K Y^(-1/4) e^(-int_0^Y (q - 1 / (4 Y))), K = (L d / s_0) (2 pi)^(-1/2) (2 s_0 mu)^(-1/4),
        // q = 1 / (4 Y) + s' / (4 s) + L d U' / (2 s) - P / 2, and (ln A_0)_eta = L_eta / L + d_eta / d - (5/4) s_eta / s_0 at Y = 0 less
        // int_0^Y q_eta. With 4 Y Phi' = 2 sqrt(2 s_0 Y) E,
        //     D(Y) = sqrt(Y / (2 s_0)) sum r_n Y^n / (2n + 1),   r = dR / E = -H / 4 - Y H' / 2 - Y E H^2 / 8 + s_0 E (ln A_0)_eta,
        // and the slope's share nears e^(Phi_1) (1 + D / sqrt(mu)) - 1.
        const SimRational quarter = core_radius_number(1ll, 4ll);
        // E_eta = (s_eta s_0 - s s_eta(0)) / (2 s_0^2 E), and with G = s_eta / E, G_eta = (s_eta_eta - G E_eta) / E
        CoreRadiusSequence &root_eta_top = s_series[ROOT_ETA_TOP];
        core_radius_taken(&root_eta_top, span + 1u, 1);
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            core_radius_pushed(
                &root_eta_top,
                core_radius_over(
                    sim_rational_difference(core_radius_times(transport_eta.value[power], transport.value[0]),
                                            core_radius_times(transport.value[power], transport_eta.value[0])),
                    core_radius_times(two, core_radius_times(transport.value[0], transport.value[0]))));
        }
        CoreRadiusSequence &root_eta = s_series[ROOT_ETA];
        core_tangent_quotient(root_eta_top, root, span, &root_eta);
        CoreRadiusSequence &quotient_eta = s_series[QUOTIENT_ETA];
        core_tangent_series_product(quotient, root_eta, span, &part);
        core_tangent_series_scaled(part, minus, &part);
        core_tangent_series_sum(transport_eta_eta, part, &part);
        core_tangent_quotient(part, root, span, &quotient_eta);
        CoreRadiusSequence &phi_eta_eta = s_series[PHI_ETA_ETA];
        core_radius_taken(&phi_eta_eta, span + 1u, 1);
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            core_radius_pushed(&phi_eta_eta, core_radius_over(quotient_eta.value[power],
                                                              core_radius_number(2ll * (long long)power + 1ll, 1ll)));
        }
        // P_eta = (E_eta H + E H_eta) / 2
        CoreRadiusSequence &phi_product_eta = s_series[PHI_PRODUCT_ETA];
        core_tangent_series_product(root_eta, phi_eta, reach, &part);
        core_tangent_series_product(root, phi_eta_eta, reach, &other);
        core_tangent_series_sum(part, other, &part);
        core_tangent_series_scaled(part, half, &phi_product_eta);
        // q_eta = [(s'_eta s - s' s_eta) / 4 + (N_eta s - N s_eta) / 2] / s^2 - P_eta / 2, N = L d U', and s^2 = s_0^2 E^4
        CoreRadiusSequence &transport_prime = s_series[TRANSPORT_PRIME];
        CoreRadiusSequence &transport_eta_prime = s_series[TRANSPORT_ETA_PRIME];
        CoreRadiusSequence &along_prime = s_series[ALONG_PRIME];
        CoreRadiusSequence &along_eta_prime = s_series[ALONG_ETA_PRIME];
        core_tangent_series_derivative(transport, reach, &transport_prime);
        core_tangent_series_derivative(transport_eta, reach, &transport_eta_prime);
        core_tangent_series_derivative(along_series, reach, &along_prime);
        core_tangent_series_derivative(along_eta_series, reach, &along_eta_prime);
        const SimRational inflow_factor = core_radius_times(l_v, d_v);
        const SimRational inflow_factor_eta = core_radius_plus(core_radius_times(l_eta, d_v), core_radius_times(l_v, d_eta));
        CoreRadiusSequence &inflow_coefficient = s_series[INFLOW_COEFFICIENT];
        CoreRadiusSequence &inflow_coefficient_eta = s_series[INFLOW_COEFFICIENT_ETA];
        core_tangent_series_scaled(along_prime, inflow_factor, &inflow_coefficient);
        core_tangent_series_scaled(along_prime, inflow_factor_eta, &part);
        core_tangent_series_scaled(along_eta_prime, inflow_factor, &other);
        core_tangent_series_sum(part, other, &inflow_coefficient_eta);
        CoreRadiusSequence &prime_part = s_series[PRIME_PART];
        core_tangent_series_product(transport_eta_prime, transport, reach, &part);
        core_tangent_series_product(transport_prime, transport_eta, reach, &other);
        core_tangent_series_scaled(other, minus, &other);
        core_tangent_series_sum(part, other, &prime_part);
        CoreRadiusSequence &inflow_part = s_series[INFLOW_PART];
        core_tangent_series_product(inflow_coefficient_eta, transport, reach, &part);
        core_tangent_series_product(inflow_coefficient, transport_eta, reach, &other);
        core_tangent_series_scaled(other, minus, &other);
        core_tangent_series_sum(part, other, &inflow_part);
        CoreRadiusSequence &amplitude_rate_eta = s_series[AMPLITUDE_RATE_ETA];
        core_tangent_series_scaled(prime_part, quarter, &part);
        core_tangent_series_scaled(inflow_part, half, &other);
        core_tangent_series_sum(part, other, &part);
        core_tangent_series_scaled(part,
                                   sim_rational_reciprocal(core_radius_times(transport.value[0], transport.value[0])),
                                   &amplitude_rate_eta);
        for (unsigned int division = 0u; division < 4u; division += 1u)
        {
            core_tangent_quotient(amplitude_rate_eta, root, reach, &amplitude_rate_eta);
        }
        core_tangent_series_scaled(phi_product_eta, core_radius_times(minus, half), &part);
        core_tangent_series_sum(amplitude_rate_eta, part, &amplitude_rate_eta);
        // (ln A_0)_eta = (ln K)_eta - int_0^Y q_eta
        const SimRational amplitude_origin_eta =
            sim_rational_difference(core_radius_plus(core_radius_over(l_eta, l_v), core_radius_over(d_eta, d_v)),
                                    core_radius_times(core_radius_number(5ll, 4ll),
                                                      core_radius_over(transport_eta.value[0], transport.value[0])));
        CoreRadiusSequence &amplitude_eta = s_series[AMPLITUDE_ETA];
        core_tangent_series_begun(&amplitude_eta, reach + 1u, amplitude_origin_eta);
        for (unsigned int power = 1u; power <= reach; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            core_radius_pushed(&amplitude_eta,
                               core_radius_over(core_radius_times(minus, amplitude_rate_eta.value[power - 1u]),
                                                core_radius_number((long long)power, 1ll)));
        }
        // r_n = -(2n + 1) H_n / 4 - (Y E H^2)_n / 8 + s_0 (E (ln A_0)_eta)_n, and the amplitude's coefficient r_n / (2n + 1)
        CoreRadiusSequence &root_phi_square = s_series[ROOT_PHI_SQUARE];
        core_tangent_series_product(phi_eta, phi_eta, reach, &part);
        core_tangent_series_product(root, part, reach, &root_phi_square);
        CoreRadiusSequence &amplitude_part = s_series[AMPLITUDE_PART];
        core_tangent_series_product(root, amplitude_eta, reach, &part);
        core_tangent_series_scaled(part, transport.value[0], &amplitude_part);
        CoreRadiusSequence &amplitude_slope = s_series[AMPLITUDE_SLOPE];
        core_radius_taken(&amplitude_slope, reach + 1u, 1);
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            const SimRational odd = core_radius_number(2ll * (long long)power + 1ll, 1ll);
            SimRational source = core_radius_plus(
                core_radius_times(core_radius_times(minus, quarter), core_radius_times(odd, phi_eta.value[power])),
                amplitude_part.value[power]);
            if (power > 0u)
            {
                source = sim_rational_difference(
                    source, core_radius_over(root_phi_square.value[power - 1u], core_radius_number(8ll, 1ll)));
            }
            core_radius_pushed(&amplitude_slope, core_radius_over(source, odd));
        }
        // r_0 by hand: s_0 (L_eta / L + d_eta / d) - (3/2) s_eta at Y = 0
        const SimRational first_by_hand = sim_rational_difference(
            core_radius_times(transport.value[0],
                              core_radius_plus(core_radius_over(l_eta, l_v), core_radius_over(d_eta, d_v))),
            core_radius_times(core_radius_number(3ll, 2ll), transport_eta.value[0]));
        sim_check(&results, sim_rational_sign(sim_rational_difference(amplitude_slope.value[0], first_by_hand)) == 0,
                  "the amplitude's first coefficient is s_0 (L_eta / L + d_eta / d) - (3/2) s_eta at Y = 0");
        // The swirl's amplitude past Phi_1. With g = e^(sqrt(mu) Phi) A_0 (b_0 + b_1 / sqrt(mu) + ...), the terms of
        // order 1 / sqrt(mu) give 4 Y Phi' b_0' + (2 Phi' - N / Phi') b_0 = -F / Phi', F = L d (Y f)' / Y, the same
        // with the slope on and off: the swirl's share nears e^(Phi_1) - 1 as the axial's does. The terms of order 1 /
        // mu give the same operator on b_1 with the source, slope on less off,
        //     dS = -2 P ((Y b_0)' + b_0 Y l) - Y b_0 (P' + P^2 / 2) + s ((ln A_0)_eta b_0 + b_0_eta) - 2 Fs (E sum d_n
        //     Y^n + P),
        // Y l = Y A_0' / A_0 of the principal and Fs = L d (Y f)' / (2 s). With J = e^(-int_0^Y N / (2 s)) the operator
        // is (sqrt(Y) J b)' = sqrt(Y) J (source) / (4 Y Phi'). Then b_0 = -J^(-1) sum 2 (J Fs)_n Y^n / (2n + 1), -L d
        // f_0 / s_0 at Y = 0, which requires f_0 past 0 at the point, and the swirl's share is e^(Phi_1) (1 + C /
        // sqrt(mu)) - 1 with
        //     C(Y) = e^(Phi_1) sqrt(Y / (2 s_0)) sum c_n Y^n,   sum c_n Y^n = (sum (J dS / E)_n Y^n / (n + 1)) / (2 J
        //     b_0).
        const SimRational zero = core_radius_number(0ll, 1ll);
        // N / (2 s) and its eta-derivative (N_eta s - N s_eta) / (2 s^2), and J = e^w with w = -int_0^Y N / (2 s)
        CoreRadiusSequence &inflow_rate = s_series[INFLOW_RATE];
        CoreRadiusSequence &inflow_rate_eta = s_series[INFLOW_RATE_ETA];
        core_tangent_over_transport(inflow_coefficient, root, transport.value[0], reach, &part);
        core_tangent_series_scaled(part, half, &inflow_rate);
        core_tangent_over_transport(inflow_part, root, transport.value[0], reach, &part);
        core_tangent_over_transport(part, root, transport.value[0], reach, &part);
        core_tangent_series_scaled(part, half, &inflow_rate_eta);
        CoreRadiusSequence &inflow_exponent = s_series[INFLOW_EXPONENT];
        CoreRadiusSequence &inflow_exponent_eta = s_series[INFLOW_EXPONENT_ETA];
        core_tangent_series_begun(&inflow_exponent, span + 1u, zero);
        core_tangent_series_begun(&inflow_exponent_eta, span + 1u, zero);
        for (unsigned int power = 1u; power <= span; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            const SimRational order_power = core_radius_number((long long)power, 1ll);
            core_radius_pushed(&inflow_exponent,
                               core_radius_over(core_radius_times(minus, inflow_rate.value[power - 1u]), order_power));
            core_radius_pushed(
                &inflow_exponent_eta,
                core_radius_over(core_radius_times(minus, inflow_rate_eta.value[power - 1u]), order_power));
        }
        CoreRadiusSequence &inflow_exponential = s_series[INFLOW_EXPONENTIAL];
        core_tangent_series_exponential(inflow_exponent, span, &inflow_exponential);
        // Fs = L d (Y f)' / (2 s) and Fs_eta = [(L d)_eta (Y f)' + L d (Y f_eta)'] / (2 s) - Fs s_eta / s
        CoreRadiusSequence &swirl_y_prime = s_series[SWIRL_Y_PRIME];
        CoreRadiusSequence &swirl_eta_y_prime = s_series[SWIRL_ETA_Y_PRIME];
        core_radius_taken(&swirl_y_prime, span + 1u, 1);
        core_radius_taken(&swirl_eta_y_prime, span + 1u, 1);
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            const SimRational next = core_radius_number((long long)power + 1ll, 1ll);
            core_radius_pushed(&swirl_y_prime, core_radius_times(next, swirl_series.value[power]));
            core_radius_pushed(&swirl_eta_y_prime, core_radius_times(next, swirl_eta_series.value[power]));
        }
        CoreRadiusSequence &swirl_forcing = s_series[SWIRL_FORCING];
        core_tangent_series_scaled(swirl_y_prime, core_radius_times(half, inflow_factor), &part);
        core_tangent_over_transport(part, root, transport.value[0], span, &swirl_forcing);
        CoreRadiusSequence &swirl_forcing_top = s_series[SWIRL_FORCING_TOP];
        core_tangent_series_scaled(swirl_y_prime, core_radius_times(half, inflow_factor_eta), &part);
        core_tangent_series_scaled(swirl_eta_y_prime, core_radius_times(half, inflow_factor), &other);
        core_tangent_series_sum(part, other, &swirl_forcing_top);
        CoreRadiusSequence &swirl_forcing_eta = s_series[SWIRL_FORCING_ETA];
        core_tangent_over_transport(swirl_forcing_top, root, transport.value[0], span, &part);
        core_tangent_series_product(swirl_forcing, transport_eta, span, &other);
        core_tangent_over_transport(other, root, transport.value[0], span, &other);
        core_tangent_series_scaled(other, minus, &other);
        core_tangent_series_sum(part, other, &swirl_forcing_eta);
        // b_0 = -I / J and b_0_eta = (w_eta I - I_eta) / J, with I = sum 2 (J Fs)_n Y^n / (2n + 1) and I_eta from J (w_eta Fs + Fs_eta)
        CoreRadiusSequence &swirl_carried = s_series[SWIRL_CARRIED];
        CoreRadiusSequence &swirl_carried_eta = s_series[SWIRL_CARRIED_ETA];
        core_tangent_series_product(inflow_exponential, swirl_forcing, span, &swirl_carried);
        core_tangent_series_product(inflow_exponent_eta, swirl_forcing, span, &part);
        core_tangent_series_sum(part, swirl_forcing_eta, &part);
        core_tangent_series_product(inflow_exponential, part, span, &swirl_carried_eta);
        CoreRadiusSequence &swirl_integral = s_series[SWIRL_INTEGRAL];
        CoreRadiusSequence &swirl_integral_eta = s_series[SWIRL_INTEGRAL_ETA];
        core_radius_taken(&swirl_integral, span + 1u, 1);
        core_radius_taken(&swirl_integral_eta, span + 1u, 1);
        for (unsigned int power = 0u; power <= span; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            const SimRational odd = core_radius_number(2ll * (long long)power + 1ll, 1ll);
            core_radius_pushed(&swirl_integral,
                               core_radius_over(core_radius_times(two, swirl_carried.value[power]), odd));
            core_radius_pushed(&swirl_integral_eta,
                               core_radius_over(core_radius_times(two, swirl_carried_eta.value[power]), odd));
        }
        CoreRadiusSequence &swirl_ratio = s_series[SWIRL_RATIO];
        CoreRadiusSequence &swirl_ratio_eta = s_series[SWIRL_RATIO_ETA];
        core_tangent_series_scaled(swirl_integral, minus, &part);
        core_tangent_quotient(part, inflow_exponential, span, &swirl_ratio);
        core_tangent_series_product(inflow_exponent_eta, swirl_integral, span, &part);
        core_tangent_series_scaled(swirl_integral_eta, minus, &other);
        core_tangent_series_sum(part, other, &part);
        core_tangent_quotient(part, inflow_exponential, span, &swirl_ratio_eta);
        // Y l = -1/4 - Y s' / (4 s) - Y N / (2 s), the principal's, and P = E H / 2 with P'
        CoreRadiusSequence &prime_rate = s_series[PRIME_RATE];
        core_tangent_over_transport(transport_prime, root, transport.value[0], reach, &prime_rate);
        CoreRadiusSequence &amplitude_log_rate = s_series[AMPLITUDE_LOG_RATE];
        core_tangent_series_begun(&amplitude_log_rate, reach + 1u, core_radius_times(minus, quarter));
        for (unsigned int power = 1u; power <= reach; power += 1u)
        {
            core_radius_pushed(&amplitude_log_rate,
                               sim_rational_difference(
                                   core_radius_times(core_radius_times(minus, quarter), prime_rate.value[power - 1u]),
                                   inflow_rate.value[power - 1u]));
        }
        CoreRadiusSequence &phi_product = s_series[PHI_PRODUCT];
        CoreRadiusSequence &phi_product_prime = s_series[PHI_PRODUCT_PRIME];
        core_tangent_series_product(root, phi_eta, span, &part);
        core_tangent_series_scaled(part, half, &phi_product);
        core_tangent_series_derivative(phi_product, reach, &phi_product_prime);
        // dS term by term: (Y b_0)' + b_0 Y l, Y b_0 (P' + P^2 / 2), s ((ln A_0)_eta b_0 + b_0_eta), and Fs (E sum d_n Y^n + P)
        // (Y A_0 b_0)' / A_0 = (Y b_0)' + b_0 Y l
        CoreRadiusSequence &swirl_moment_prime = s_series[SWIRL_MOMENT_PRIME];
        core_tangent_series_product(swirl_ratio, amplitude_log_rate, reach, &swirl_moment_prime);
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            swirl_moment_prime.value[power] = core_radius_plus(
                swirl_moment_prime.value[power],
                core_radius_times(core_radius_number((long long)power + 1ll, 1ll), swirl_ratio.value[power]));
        }
        CoreRadiusSequence &ratio_growth = s_series[RATIO_GROWTH];
        core_tangent_series_product(phi_product, phi_product, reach, &part);
        core_tangent_series_scaled(part, half, &part);
        core_tangent_series_sum(phi_product_prime, part, &part);
        core_tangent_series_product(swirl_ratio, part, reach, &ratio_growth);
        CoreRadiusSequence &ratio_slope = s_series[RATIO_SLOPE];
        core_tangent_series_product(amplitude_eta, swirl_ratio, reach, &part);
        core_tangent_series_sum(part, swirl_ratio_eta, &part);
        core_tangent_series_product(transport, part, reach, &ratio_slope);
        CoreRadiusSequence &ratio_forcing = s_series[RATIO_FORCING];
        core_tangent_series_product(root, amplitude_slope, reach, &part);
        core_tangent_series_sum(part, phi_product, &part);
        core_tangent_series_product(swirl_forcing, part, reach, &ratio_forcing);
        const SimRational minus_two = core_radius_number(-2ll, 1ll);
        CoreRadiusSequence &swirl_source = s_series[SWIRL_SOURCE];
        core_tangent_series_product(phi_product, swirl_moment_prime, reach, &part);
        core_tangent_series_scaled(part, minus_two, &part);
        core_tangent_series_scaled(ratio_forcing, minus_two, &other);
        core_tangent_series_sum(ratio_slope, other, &other);
        core_tangent_series_sum(part, other, &swirl_source);
        for (unsigned int power = 1u; power <= reach; power += 1u)
        {
            swirl_source.value[power] =
                sim_rational_difference(swirl_source.value[power], ratio_growth.value[power - 1u]);
        }
        // sum c_n Y^n = (sum (J dS / E)_n Y^n / (n + 1)) / (2 J b_0)
        CoreRadiusSequence &source_carried = s_series[SOURCE_CARRIED];
        core_tangent_series_product(inflow_exponential, swirl_source, reach, &part);
        core_tangent_quotient(part, root, reach, &source_carried);
        CoreRadiusSequence &source_integral = s_series[SOURCE_INTEGRAL];
        core_radius_taken(&source_integral, reach + 1u, 1);
        for (unsigned int power = 0u; power <= reach; power += 1u)
        {
            // the power is at max the cfg's order, far inside a long long
            core_radius_pushed(&source_integral, core_radius_over(source_carried.value[power],
                                                                  core_radius_number((long long)power + 1ll, 1ll)));
        }
        const SimRational ratio_zero = swirl_ratio.value[0];
        CoreRadiusSequence &swirl_slope = s_series[SWIRL_SLOPE];
        core_tangent_series_scaled(source_integral, sim_rational_reciprocal(core_radius_times(two, ratio_zero)), &part);
        core_tangent_series_product(inflow_exponential, swirl_ratio, reach, &other);
        core_tangent_series_scaled(other, sim_rational_reciprocal(ratio_zero), &other);
        core_tangent_quotient(part, other, reach, &swirl_slope);
        // b_0 and c_0 by hand at Y = 0: -L d f_0 / s_0 and (3/2) s_0 (L_eta / L + d_eta / d) + (1/2) s_0 f_eta / f_0 - 2 s_eta
        const SimRational ratio_by_hand = core_radius_over(
            core_radius_times(minus, core_radius_times(inflow_factor, swirl_series.value[0])), transport.value[0]);
        const SimRational inflow_factor_log_eta = core_radius_plus(core_radius_over(l_eta, l_v), core_radius_over(d_eta, d_v));
        const SimRational swirl_by_hand = sim_rational_difference(
            core_radius_plus(core_radius_times(core_radius_times(core_radius_number(3ll, 2ll), transport.value[0]),
                                               inflow_factor_log_eta),
                             core_radius_times(core_radius_times(half, transport.value[0]),
                                               core_radius_over(swirl_eta_series.value[0], swirl_series.value[0]))),
            core_radius_times(two, transport_eta.value[0]));
        sim_check(&results,
                  (sim_rational_sign(sim_rational_difference(ratio_zero, ratio_by_hand)) == 0) &&
                      (sim_rational_sign(sim_rational_difference(swirl_slope.value[0], swirl_by_hand)) == 0),
                  "the swirl's b_0 is -L d f_0 / s_0 and its first coefficient (3/2) s_0 (L_eta / L + d_eta / d) + "
                  "(1/2) s_0 f_eta / f_0 - 2 s_eta at Y = 0");
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "phase at the vertex eta ");
            record_rational(&line.line, eta_v);
            mmgr_line_put(&line.line, " of E_rho, rho ");
            record_rational(&line.line, phase_rho);
            mmgr_line_put(&line.line, ", to order ");
            mmgr_line_decimal(&line.line, reach, 1u);
            mmgr_line_put(&line.line, ": s_0 ");
            record_rational(&line.line, transport.value[0]);
            mmgr_line_put(&line.line, ", s_eta at Y = 0 ");
            record_rational(&line.line, transport_eta.value[0]);
            mmgr_line_put(&line.line, "; then for each n the coefficient of Y^n of Phi_1");
            record_line_ended(record, &line);
            for (unsigned int power = 1u; power <= reach; power += 1u)
            {
                core_tangent_power_recorded(record, power, phi_slope.value[power]);
            }
            record_text(record, "Y, Phi_1(Y) and the last order's term over the sum");
            for (unsigned int y_at = 0u; y_at < phase_y.count; y_at += 1u)
            {
                const SimRational y_point = phase_y.value[y_at];
                SimRational last_power = one;
                for (unsigned int power = 0u; power < reach; power += 1u)
                {
                    last_power = core_radius_times(last_power, y_point);
                }
                const SimRational sum = core_tangent_sum_at(phi_slope, y_point);
                record_line_begun(&line);
                mmgr_line_put(&line.line, "  ");
                record_rational(&line.line, y_point);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line, sum);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line,
                                core_radius_over(core_radius_times(phi_slope.value[reach], last_power), sum));
                record_line_ended(record, &line);
            }
            record_text(record, "the amplitude past Phi_1, D(Y) = sqrt(Y / (2 s_0)) sum d_n Y^n: for each n the coefficient d_n");
            for (unsigned int power = 0u; power <= reach; power += 1u)
            {
                core_tangent_power_recorded(record, power, amplitude_slope.value[power]);
            }
            record_text(record, "Y, sum d_n Y^n and the last order's term over the sum");
            for (unsigned int y_at = 0u; y_at < phase_y.count; y_at += 1u)
            {
                const SimRational y_point = phase_y.value[y_at];
                SimRational last_power = one;
                for (unsigned int power = 0u; power < reach; power += 1u)
                {
                    last_power = core_radius_times(last_power, y_point);
                }
                const SimRational sum = core_tangent_sum_at(amplitude_slope, y_point);
                record_line_begun(&line);
                mmgr_line_put(&line.line, "  ");
                record_rational(&line.line, y_point);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line, sum);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line,
                                core_radius_over(core_radius_times(amplitude_slope.value[reach], last_power), sum));
                record_line_ended(record, &line);
            }
            record_text(record, "the swirl's amplitude past Phi_1, C(Y) = e^(Phi_1) sqrt(Y / (2 s_0)) sum c_n Y^n: for each n the coefficient c_n");
            for (unsigned int power = 0u; power <= reach; power += 1u)
            {
                core_tangent_power_recorded(record, power, swirl_slope.value[power]);
            }
            record_text(record, "Y, sum c_n Y^n and the last order's term over the sum");
            for (unsigned int y_at = 0u; y_at < phase_y.count; y_at += 1u)
            {
                const SimRational y_point = phase_y.value[y_at];
                SimRational last_power = one;
                for (unsigned int power = 0u; power < reach; power += 1u)
                {
                    last_power = core_radius_times(last_power, y_point);
                }
                const SimRational sum = core_tangent_sum_at(swirl_slope, y_point);
                record_line_begun(&line);
                mmgr_line_put(&line.line, "  ");
                record_rational(&line.line, y_point);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line, sum);
                mmgr_line_ch(&line.line, ' ');
                record_rational(&line.line,
                                core_radius_over(core_radius_times(swirl_slope.value[reach], last_power), sum));
                record_line_ended(record, &line);
            }
        }
        mmgr_line_put(&results.line, "  phase at the vertex written to the record\n");
        sim_flush(&results);
        for (unsigned int each = 0u; each < SERIES; each += 1u)
        {
            core_radius_released(&s_series[each]);
        }
        core_radius_orders_released(&base);
    }
    core_radius_orders_released(&exact);
    if (core_radius_unmet())
    {
        mmgr_line_put(&results.line,
                      "  the pool did not meet a cell of the weights: run with a larger g_program_pool_bytes\n");
    }
    const int fits = !run_cfg_short() && !record_short() && (g_sim_rational_wide == 0) && !core_radius_unmet();
    mmgr_line_put(&results.line, fits ? "  every value is exact and stored in the build's width\n"
                                       : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, fits, "every value stored");
    const int recorded = (record != NULL) && record_close(record);
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core tangent");
}
