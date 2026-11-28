// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius.cu: a radius in X the axis core's series is proved to reach, its fields bounded order by order on a
// family of ellipses in eta, every bound an exact rational
#include "run_cfg.h"

#include "record.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "core_radius_weights.h"
#include "../../../../includes/platform/platform.h"

// the bytes of the pool: four sets of orders to the reach, at 1 KB a rational, and the cells their rule takes
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// an empty sequence, with no cell
#define CORE_RADIUS_NONE {NULL, 0u, 0u}

// The ellipse E_rho is the image of the circle |z| = rho under eta = (z + 1/z) / 2, drawn over the segment
// [-1, 1] with its feet at eta = -1 and eta = +1, where d = 1 - eta^2 is 0. A field f = sum f_m T_m(eta), its
// Chebyshev weights given by the cfg, is bounded by ||f||_rho = sum |f_m| rho^m. For 0 <= h <= 1/2 and
// 1 < rho_min <= rho' < rho <= rho0:
//     ||f g|| <= ||f|| ||g||,   ||eta f|| <= rho0 ||f||,   ||d f|| <= (1 + rho0^2) / 2 ||f||,
//     ||L^-1 f|| <= l ||f||, l = 1 / ((1 - h) - h rho0^2),
//     ||f'||_rho' <= K / (rho - rho') ||f||_rho, K = rho0^2 / (rho_min^2 - 1),
//     ||d f'||_rho' <= ((1 + rho0^2) / 4) / (rho - rho') ||f||_rho,
// the third from T_m' = m U_(m-1), ||U_(m-1)||_rho' <= 2 rho'^(m+1) / (rho'^2 - 1) and m q^m <= 1 / (2 (1 - q)), the
// last from (1 - eta^2) T_m' = m (T_(m-1) - T_(m+1)) / 2: with the derivative bounded at the feet by d, its bound reads
// no rho_min. The order k is bounded on its own scale of ellipses, |f|_k = sup over rho in [rho_min, rho0) of
// ||f||_rho (rho0 - rho)^k, which gives |f g|_(j+k) <= |f|_j |g|_k, |f|_(k+1) <= Delta |f|_k with
// Delta = rho0 - rho_min, |f'|_(k+1) <= 3 K (k + 1) |f|_k and |d f'|_(k+1) <= 3 (1 + rho0^2) / 4 (k + 1) |f|_k.
// The rule of core_rule.cu then bounds a_k = |F_k|_k, u_k = |U_k|_k, p_k = |P_k|_k and w_k = |v_k|_(k+1) order by
// order from the data: the witness, exact to the order N. Each d f' is bounded both ways, by (1 + rho0^2) / 2 times
// 3 K and by the feet's 3 (1 + rho0^2) / 4, and each gives its own witness. The weights of each order retained apart,
// as magnitudes and exact, give two more, read on the ellipse only where the order is bounded. The exact weights to N,
// carried past N to the cfg's carry by magnitudes mode by mode, give a last one: each carried order bounds the exact
// one where its weight sits, and each is rounded up to a multiple of 2^-bits, a bound stored in a length that does not
// grow with the order.
// The proof: with B_x = max over k <= N of x_k r^k (k + 1), B_w raised to at least C_w B_u, C_w = l (2 rho0 Delta + E),
// E the constant of d f', and B_p to at least 2 r Delta B_a^2 H_(N+1) / (N + 1), two inequalities at k = N, each a sum
// of terms that fall as k grows, carry x_k <= B_x r^-k / (k + 1) from every order to the next past N. The 1 / (k + 1)
// is what the rule's 2 (k+1)(k+2) and 2 (k+1)^2 leave over the Cauchy sums, and with it no part of the proof stays
// as N grows: r is bounded by the witness alone. On E_rho the core's series is bounded by
// sum B_a (X / (r (rho0 - rho)))^k / (k + 1) and reaches every X < r (rho0 - rho).
// Checks:
// 1. Every order's weights, carried ones too, are within the length the rule fixes, the length the proof on one
//    ellipse reads.
// 2. Every ellipse of the cfg is one the bounds are valid on.
// 3. Every bound of the witness and the proof is exact and stored in the build's width.
// 4. The witness and the proof are written whole to the cfg's record, with the radius each proves.
// 5. An X inside a proved radius has F_low past 0, and each medium of the cfg has its walls written there.
// 6. The carry on the device bounds every sum below the product of its primes, and is the host's to the last bit at
//    every order both reach.
// 7. Where the cfg contains a principal system, s_eff and (Y F)' are past 0 on its [0, Y_1] at every ellipse's vertex.
// The request: core_radius <cfg>. The norms of the reach's orders on E_rho carry rho^m to its last mode, and request
// the width of 256 limbs.
//     SIM_EXACT_LIMBS=256 bash examples/navier_stokes/run.sh core_radius examples/navier_stokes/cfg/core_radius.cfg

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
    // a, u and p to order + 1, w to order, each a cell of the temporary tier that the caller gives back
    CoreRadiusSequence *const fields[4] = {a, u, p, w};
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        core_radius_released(fields[field]);
        core_radius_taken(fields[field], (field < 3u) ? (order + 2u) : (order + 1u), 1);
    }
    core_radius_pushed(a, a0);
    core_radius_pushed(u, u0);
    core_radius_pushed(p, p0);
    for (unsigned int k = 0u; (a->count != 0u) && (k <= order); k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        // (k + 1) w_k = l [(2A + 2k) rho0 Delta + E (k + 1)] u_k
        core_radius_pushed(
            w, core_radius_over(
                   core_radius_times(core_radius_turned(scale, core_radius_times(two, scale->a), order_k), u->value[k]),
                   next));
        // L^-1 ((k + 1 + h) F_k + D eta F_k') and L^-1 ((k + A) U_k + D eta U_k')
        SimRational angular = core_radius_times(scale->l, core_radius_plus(core_radius_times(core_radius_plus(next, scale->h), scale->delta), core_radius_times(d_rho_k3, next)));
        angular = core_radius_times(angular, a->value[k]);
        SimRational along = core_radius_times(scale->l, core_radius_plus(core_radius_times(core_radius_plus(order_k, scale->a), scale->delta), core_radius_times(d_rho_k3, next)));
        along = core_radius_times(along, u->value[k]);
        SimRational square = core_radius_number(0ll, 1ll);
        for (unsigned int i = 0u; i <= k; i += 1u)
        {
            const SimRational rest = core_radius_number((long long)(k - i), 1ll);
            // v_i (k - i + 1) F_(k-i) + U_i (Z_{-(A+1/2)} F)_(k-i)
            angular =
                core_radius_plus(angular, core_radius_times(core_radius_times(core_radius_plus(rest, one), w->value[i]),
                                                            a->value[k - i]));
            angular = core_radius_plus(
                angular, core_radius_times(u->value[i], core_radius_times(core_radius_turned(scale, angular_c, rest),
                                                                          a->value[k - i])));
            // v_i (k - i) U_(k-i) + U_i (Z_{-A} U)_(k-i)
            along = core_radius_plus(along, core_radius_times(core_radius_times(rest, w->value[i]), u->value[k - i]));
            along = core_radius_plus(
                along, core_radius_times(u->value[i],
                                         core_radius_times(core_radius_turned(scale, along_c, rest), u->value[k - i])));
            square = core_radius_plus(square, core_radius_times(a->value[i], a->value[k - i]));
        }
        // (Z_{-2A} Pi)_k
        along = core_radius_plus(along, core_radius_times(core_radius_turned(scale, pressure_c, order_k), p->value[k]));
        core_radius_pushed(
            a, core_radius_over(angular, core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))));
        core_radius_pushed(u, core_radius_over(along, core_radius_times(two, core_radius_times(next, next))));
        core_radius_pushed(p, core_radius_over(core_radius_times(scale->delta, square), next));
    }
}




// the scalars the proof reads on one ellipse: l^(2k) ||.||_rho0 Delta^k, and l^(2k+2) ||w_k||_rho0 Delta^(k+1)
static void core_radius_weights_witness(const CoreRadiusScale *scale, const CoreRadiusOrders *orders, unsigned int order, CoreRadiusSequence *a,
                                        CoreRadiusSequence *u, CoreRadiusSequence *p, CoreRadiusSequence *w)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const CoreRadiusWeights *const f = orders->f;
    const CoreRadiusWeights *const g = orders->g;
    const CoreRadiusWeights *const q = orders->q;
    const CoreRadiusWeights *const inflow = orders->inflow;
    // a, u, p and w to order, each a cell of the temporary tier that the caller gives back
    CoreRadiusSequence *const fields[4] = {a, u, p, w};
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        core_radius_released(fields[field]);
        core_radius_taken(fields[field], order + 1u, 1);
    }
    SimRational power = one;
    const SimRational l_square_delta = core_radius_times(core_radius_times(scale->l, scale->l), scale->delta);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        core_radius_pushed(a, core_radius_times(power, core_radius_norm(&f[k], scale->rho)));
        core_radius_pushed(u, core_radius_times(power, core_radius_norm(&g[k], scale->rho)));
        core_radius_pushed(p, core_radius_times(power, core_radius_norm(&q[k], scale->rho)));
        core_radius_pushed(
            w, core_radius_times(core_radius_times(power, l_square_delta), core_radius_norm(&inflow[k], scale->rho)));
        power = core_radius_times(power, l_square_delta);
    }
}

// B_x = max over k <= order of x_k r^k (k + 1)
static SimRational core_radius_max_scaled(const CoreRadiusSequence &sequence, SimRational r, unsigned int order)
{
    SimRational max = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        max = core_radius_max(max, core_radius_times(core_radius_times(sequence.value[k], power),
                                                     core_radius_number((long long)k + 1ll, 1ll)));
        power = core_radius_times(power, r);
    }
    return max;
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
    const SimRational b_a = core_radius_max_scaled(a, r, order);
    const SimRational b_u = core_radius_max_scaled(u, r, order);
    if (sim_rational_sign(b_u) <= 0)
    {
        return 0;
    }
    // w_k <= C_w u_k past N with A <= 1, and P_(k+1) past N needs B_p >= 2 r Delta B_a^2 H_(N+1) / (N + 1)
    const SimRational c_w = core_radius_times(scale->l, core_radius_plus(core_radius_times(two, rho_delta), scale->feet));
    const SimRational b_w = core_radius_max(core_radius_max_scaled(w, r, order), core_radius_times(c_w, b_u));
    const SimRational pressure_floor = core_radius_over(core_radius_times(core_radius_times(two, r), core_radius_times(scale->delta, core_radius_times(core_radius_times(b_a, b_a), harmonic))), n1);
    const SimRational b_p = core_radius_max(core_radius_max_scaled(p, r, order), pressure_floor);
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

// The proof on one ellipse. Every term of the rule raises a numerator's degree by at max 3, and a product of two by
// at max 4: f_k, u_k and p_k have degree at max G_k = g0 + (g0 + 4) k and w_k at max G_k + 3, g0 the data's
// degree: the length of each order's weights is fixed by the rule, and on E_rho a derivative costs that length,
// ||g'|| <= gamma G ||g||, gamma = 2 rho / (rho^2 - 1), and ||d g'|| <= phi G ||g||, phi = (rho + 1/rho) / 2. With
// lambda = (1 - h) + h rho^2, delta = (1 + rho^2) / 2 and beta = 2 rho lambda + lambda phi (g0 + 4) + 8 h rho delta,
// the Z parts of order j are at max (alpha + beta j) times their field, alpha = c rho lambda + lambda phi g0. The
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
        sum = core_radius_plus(sum, core_radius_times(weight, core_radius_times(n.value[k], power)));
        power = core_radius_times(power, r);
    }
    return sum;
}

// max over split <= k <= order of n_k r^k
static SimRational core_radius_high(const CoreRadiusSequence &n, SimRational r, unsigned int split, unsigned int order)
{
    SimRational max = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        if (k >= split)
        {
            max = core_radius_max(max, core_radius_times(n.value[k], power));
        }
        power = core_radius_times(power, r);
    }
    return max;
}

// the numbers the proof on one ellipse reads at r: the rule's constants, n_k r^k below the split, and the bounds past it
typedef struct
{
    SimRational rate;
    SimRational lambda;
    SimRational angular_a0;
    SimRational along_a0;
    SimRational a1;
    SimRational alpha_angular;
    SimRational alpha_along;
    SimRational alpha_pressure;
    SimRational beta;
    CoreRadiusSequence f;
    CoreRadiusSequence u;
    CoreRadiusSequence w;
    SimRational b_f;
    SimRational b_u;
    SimRational b_p;
    SimRational b_w;
} CoreRadiusProof;

// n_k r^k for k < split into `scaled`, its cell before given back and a cell of the temporary tier taken at split
static void core_radius_below_split(const CoreRadiusSequence &n, SimRational r, unsigned int split,
                                    CoreRadiusSequence *scaled)
{
    core_radius_released(scaled);
    core_radius_taken(scaled, split, 1);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int k = 0u; k < split; k += 1u)
    {
        core_radius_pushed(scaled, core_radius_times(n.value[k], power));
        power = core_radius_times(power, r);
    }
}

// the cells of the lists of `proof` given back
static void core_radius_proof_released(CoreRadiusProof *proof)
{
    core_radius_released(&proof->f);
    core_radius_released(&proof->u);
    core_radius_released(&proof->w);
}

// 1 where the proof on one ellipse carries n_k <= B r^-k from N to every order past it, the numbers it reads in proof
static int core_radius_fixed_proof(const CoreRadiusScale *scale, const CoreRadiusFixed *fixed, unsigned int g0, const CoreRadiusSequence &nf,
                                   const CoreRadiusSequence &nu, const CoreRadiusSequence &np, const CoreRadiusSequence &nw, unsigned int order,
                                   unsigned int split, SimRational r, CoreRadiusProof *proof)
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
    const SimRational b_w = core_radius_max(core_radius_high(nw, r, split, order), core_radius_times(core_radius_max(fixed->alpha_along, fixed->beta), b_u));
    const SimRational lambda_square = core_radius_times(fixed->lambda, fixed->lambda);
    const SimRational pressure_floor = core_radius_times(core_radius_times(r, lambda_square), core_radius_plus(core_radius_over(core_radius_times(core_radius_times(two, f0), b_f), n1), core_radius_times(b_f, b_f)));
    const SimRational b_p = core_radius_max(core_radius_high(np, r, split, order), pressure_floor);
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
    proof->rate = r;
    proof->lambda = fixed->lambda;
    proof->angular_a0 = angular_a0;
    proof->along_a0 = along_a0;
    proof->a1 = a1;
    proof->alpha_angular = fixed->alpha_angular;
    proof->alpha_along = fixed->alpha_along;
    proof->alpha_pressure = fixed->alpha_pressure;
    proof->beta = fixed->beta;
    core_radius_below_split(nf, r, split, &proof->f);
    core_radius_below_split(nu, r, split, &proof->u);
    core_radius_below_split(nw, r, split, &proof->w);
    proof->b_f = b_f;
    proof->b_u = b_u;
    proof->b_p = b_p;
    proof->b_w = b_w;
    const SimRational limit = sim_rational_reciprocal(r);
    return (sim_rational_sign(sim_rational_difference(limit, angular)) >= 0) && (sim_rational_sign(sim_rational_difference(limit, along)) >= 0);
}

// the Lean file at the cfg's member `member`, a path from the directory the cfg is in, written whole: 1 where every
// byte was written
static int core_radius_lean_write(const char *cfg_path, const RunCfg *cfg, const char *member, PlatformText *text)
{
    char path[PLATFORM_PATH_BYTES];
    const char *const bytes = mmgr_line_ok(&text->line) ? platform_text_bytes(text) : NULL;
    if ((bytes == NULL) || (run_cfg_beside(cfg_path, cfg, member, path, sizeof(path)) == 0))
    {
        return 0;
    }
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    // the Lean file is far below what a size_t counts
    fwrite(bytes, 1u, (size_t)platform_text_size(text), file);
    const int written = ferror(file) == 0;
    return (fclose(file) == 0) && written;
}

// the bytes the Lean file grows to at max
#define CORE_RADIUS_LEAN_MAX (1ULL << 30u)

// the text of the Lean file, of the movable tier, begun empty
static void core_radius_lean_begun(PlatformText *text)
{
    platform_text_begun(text, CORE_RADIUS_LEAN_MAX, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
}

// the exact rationals of a sequence as a Lean list, after the bytes of `line`
static void core_radius_lean_list(VerbaLinea *line, const CoreRadiusSequence &n)
{
    mmgr_line_ch(line, '[');
    for (unsigned int k = 0u; k < n.count; k += 1u)
    {
        mmgr_line_put(line, (k == 0u) ? "" : ", ");
        record_rational(line, n.value[k]);
    }
    mmgr_line_ch(line, ']');
}

// the line "<before><value><after>" after the bytes of `line`
static void core_radius_lean_field(VerbaLinea *line, const char *before, SimRational value, const char *after)
{
    mmgr_line_put(line, before);
    record_rational(line, value);
    mmgr_line_put(line, after);
}

// the proof on one ellipse as a Lean witness `name` and the theorem that its numbers pass the checks, after the bytes
// of `lean`
static void core_radius_lean(PlatformText *lean, const char *name, const char *title, unsigned int split,
                             unsigned int order, const CoreRadiusProof *proof)
{
    VerbaLinea *const line = &lean->line;
    mmgr_line_put(line, "/-- ");
    mmgr_line_put(line, title);
    mmgr_line_put(line, " -/\ndef ");
    mmgr_line_put(line, name);
    mmgr_line_put(line, " : Witness where\n  rule :=\n");
    core_radius_lean_field(line, "    { rate := ", proof->rate, "\n");
    core_radius_lean_field(line, "      lambda := ", proof->lambda, "\n");
    core_radius_lean_field(line, "      angular_a0 := ", proof->angular_a0, "\n");
    core_radius_lean_field(line, "      along_a0 := ", proof->along_a0, "\n");
    core_radius_lean_field(line, "      a1 := ", proof->a1, "\n");
    core_radius_lean_field(line, "      alpha_angular := ", proof->alpha_angular, "\n");
    core_radius_lean_field(line, "      alpha_along := ", proof->alpha_along, "\n");
    core_radius_lean_field(line, "      alpha_pressure := ", proof->alpha_pressure, "\n");
    core_radius_lean_field(line, "      beta := ", proof->beta, " }\n");
    mmgr_line_put(line, "  split := ");
    mmgr_line_decimal(line, split, 1u);
    mmgr_line_put(line, "\n  order := ");
    mmgr_line_decimal(line, order, 1u);
    mmgr_line_put(line, "\n  f := ");
    core_radius_lean_list(line, proof->f);
    mmgr_line_put(line, "\n  u := ");
    core_radius_lean_list(line, proof->u);
    mmgr_line_put(line, "\n  w := ");
    core_radius_lean_list(line, proof->w);
    mmgr_line_ch(line, '\n');
    core_radius_lean_field(line, "  bf := ", proof->b_f, "\n");
    core_radius_lean_field(line, "  bu := ", proof->b_u, "\n");
    core_radius_lean_field(line, "  bp := ", proof->b_p, "\n");
    core_radius_lean_field(line, "  bw := ", proof->b_w, "\n\n");
    // the kernel evaluates every inequality over Q, a split's sums of rationals thousands of bits wide among them
    mmgr_line_put(line, "theorem ");
    mmgr_line_put(line, name);
    mmgr_line_put(line, "_checks : ");
    mmgr_line_put(line, name);
    mmgr_line_put(line, ".Checks := by\n  unfold Witness.Checks\n  decide +kernel\n\n");
}

// the line "    <first> <second> <third> <fourth>" of the record
static void core_radius_four_recorded(FILE *record, SimRational first, SimRational second, SimRational third,
                                      SimRational fourth)
{
    PlatformText line;
    record_line_begun(&line);
    mmgr_line_put(&line.line, "    ");
    record_rational(&line.line, first);
    mmgr_line_ch(&line.line, ' ');
    record_rational(&line.line, second);
    mmgr_line_ch(&line.line, ' ');
    record_rational(&line.line, third);
    mmgr_line_ch(&line.line, ' ');
    record_rational(&line.line, fourth);
    record_line_ended(record, &line);
}

// the line "  proved r <proved>, radius <radius>" of the record
static void core_radius_proved_recorded(FILE *record, SimRational proved, SimRational radius)
{
    PlatformText line;
    record_line_begun(&line);
    mmgr_line_put(&line.line, "  proved r ");
    record_rational(&line.line, proved);
    mmgr_line_put(&line.line, ", radius ");
    record_rational(&line.line, radius);
    record_line_ended(record, &line);
}

// sum m |c_m| rho^m / (length sum |c_m| rho^m), 0 for weights all 0
static SimRational core_radius_place(const CoreRadiusWeights &weights, SimRational rho, unsigned int length)
{
    SimRational moment = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int m = 0u; m < weights.count; m += 1u)
    {
        moment = core_radius_plus(moment,
                                  core_radius_times(core_radius_number((long long)m, 1ll),
                                                    core_radius_times(sim_rational_absolute(weights.value[m]), power)));
        power = core_radius_times(power, rho);
    }
    const SimRational mass =
        core_radius_times(core_radius_number((long long)length, 1ll), core_radius_norm(&weights, rho));
    return (sim_rational_sign(mass) > 0) ? core_radius_over(moment, mass) : core_radius_number(0ll, 1ll);
}

// the largest r = j step the proof on E_rho is valid at, the radius r / l^2 it proves, with the norms written to the
// record and the numbers it reads at r appended to `lean` as the witness `name`; the numbers and the norms in
// `rate_proof` and `norms`, the rate in `rate_proof` 0 where no r is proved
static SimRational core_radius_fixed_rate(const CoreRadiusScale *scale, const CoreRadiusOrders *orders, unsigned int g0,
                                          unsigned int order, unsigned int split, SimRational step,
                                          unsigned long long steps, FILE *record, const char *name, const char *title,
                                          PlatformText *lean, CoreRadiusProof *rate_proof, CoreRadiusSequence *norms)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational rho = scale->rho;
    const SimRational rho_square = core_radius_times(rho, rho);
    // On E_rho in xi = eta / a: ||eta f|| <= a rho ||f||, ||L f|| <= lambda ||f|| with lambda = (1 - h a^2) + h a^2 rho^2,
    // ||eta^2 f|| <= a^2 (1 + rho^2) / 2 ||f||, ||d f|| <= ((1 - a^2 / 2) + a^2 rho^2 / 2) ||f||, ||f_xi|| <= gamma G ||f||
    // and ||d f_eta|| <= phi G ||f|| with phi = ((rho + 1/rho) / 2 + (1 - a^2) gamma (1 + rho^2) / 2) / a, the feet's part
    // and the rest of d f_eta = (1 / a) [(1 - xi^2) f_xi + (1 - a^2) xi^2 f_xi]. D eta L f_eta reads a rho / a = rho.
    const SimRational cut_square = core_radius_times(scale->cut, scale->cut);
    const SimRational h_square = core_radius_times(scale->h, cut_square);
    const SimRational rest = sim_rational_difference(one, cut_square);
    CoreRadiusFixed fixed;
    fixed.lambda = core_radius_plus(sim_rational_difference(one, h_square), core_radius_times(h_square, rho_square));
    fixed.delta = core_radius_times(cut_square, core_radius_over(core_radius_plus(one, rho_square), two));
    const SimRational d_bound = core_radius_plus(sim_rational_difference(one, core_radius_over(cut_square, two)), core_radius_over(core_radius_times(cut_square, rho_square), two));
    fixed.gamma = core_radius_over(core_radius_times(two, rho), sim_rational_difference(rho_square, one));
    const SimRational feet_part = core_radius_over(core_radius_plus(rho, sim_rational_reciprocal(rho)), two);
    const SimRational rest_part = core_radius_times(core_radius_times(rest, fixed.gamma), core_radius_over(core_radius_plus(one, rho_square), two));
    fixed.phi = core_radius_over(core_radius_plus(feet_part, rest_part), scale->cut);
    const SimRational rho_eta = core_radius_times(scale->cut, rho);
    const SimRational rho_lambda = core_radius_times(rho_eta, fixed.lambda);
    const SimRational lambda_phi = core_radius_times(fixed.lambda, fixed.phi);
    const SimRational g = core_radius_number((long long)g0, 1ll);
    fixed.beta = core_radius_plus(core_radius_plus(core_radius_times(two, rho_lambda), core_radius_times(lambda_phi, core_radius_number((long long)g0 + 4ll, 1ll))),
                                  core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), core_radius_times(rho_eta, d_bound)));
    fixed.alpha_angular = core_radius_plus(core_radius_times(core_radius_plus(two, core_radius_times(two, scale->h)), rho_lambda), core_radius_times(lambda_phi, g));
    fixed.alpha_along = core_radius_plus(core_radius_times(core_radius_times(two, scale->a), rho_lambda), core_radius_times(lambda_phi, g));
    fixed.alpha_pressure = core_radius_plus(core_radius_times(core_radius_times(core_radius_number(4ll, 1ll), scale->a), rho_lambda), core_radius_times(lambda_phi, g));
    // the norms to order, each a cell of the temporary tier: nf goes to `norms`, and the call gives the rest back
    CoreRadiusSequence nf = CORE_RADIUS_NONE;
    CoreRadiusSequence nu = CORE_RADIUS_NONE;
    CoreRadiusSequence np = CORE_RADIUS_NONE;
    CoreRadiusSequence nw = CORE_RADIUS_NONE;
    core_radius_taken(&nf, order + 1u, 1);
    core_radius_taken(&nu, order + 1u, 1);
    core_radius_taken(&np, order + 1u, 1);
    core_radius_taken(&nw, order + 1u, 1);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        core_radius_pushed(&nf, core_radius_norm(&orders->f[k], rho));
        core_radius_pushed(&nu, core_radius_norm(&orders->g[k], rho));
        core_radius_pushed(&np, core_radius_norm(&orders->q[k], rho));
        core_radius_pushed(&nw, core_radius_norm(&orders->inflow[k], rho));
    }
    CoreRadiusProof proof = {};
    unsigned long long low = 0ull;
    unsigned long long high = steps + 1ull;
    while (high - low > 1ull)
    {
        const unsigned long long middle = low + (high - low) / 2ull;
        if (core_radius_fixed_proof(scale, &fixed, g0, nf, nu, np, nw, order, split, core_radius_times(core_radius_number((long long)middle, 1ll), step), &proof))
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
    core_radius_proof_released(rate_proof);
    rate_proof->rate = core_radius_number(0ll, 1ll);
    if ((low > 0ull) && core_radius_fixed_proof(scale, &fixed, g0, nf, nu, np, nw, order, split, proved, &proof))
    {
        core_radius_lean(lean, name, title, split, order, &proof);
        // the proof's lists go to `rate_proof`, which gives them back
        *rate_proof = proof;
        proof.f = CoreRadiusSequence CORE_RADIUS_NONE;
        proof.u = CoreRadiusSequence CORE_RADIUS_NONE;
        proof.w = CoreRadiusSequence CORE_RADIUS_NONE;
    }
    core_radius_proof_released(&proof);
    if (record != NULL)
    {
        PlatformText line;
        record_line_begun(&line);
        mmgr_line_put(&line.line, "  one ellipse, split ");
        mmgr_line_decimal(&line.line, split, 1u);
        mmgr_line_put(&line.line, ": ||f_k||, ||u_k||, ||p_k||, ||w_k|| on E_rho0 to order ");
        mmgr_line_decimal(&line.line, order, 1u);
        record_line_ended(record, &line);
        for (unsigned int k = 0u; k <= order; k += 1u)
        {
            core_radius_four_recorded(record, nf.value[k], nu.value[k], np.value[k], nw.value[k]);
        }
        core_radius_proved_recorded(record, proved, radius);
        // each order's weights read as amplitudes in [0, 1] over a place t = m / G_k in [0, 1]: where the order
        // contains its weight, and so the share of its length a derivative of it costs
        record_text(record, "  place of f_k and u_k on the 0-1 scale, sum m |c_m| rho^m / (G_k sum |c_m| rho^m)");
        for (unsigned int k = 1u; k <= order; k += 1u)
        {
            const unsigned int length = g0 + (g0 + 4u) * k;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "    ");
            mmgr_line_decimal(&line.line, k, 1u);
            mmgr_line_ch(&line.line, ' ');
            record_rational(&line.line, core_radius_place(orders->f[k], rho, length));
            mmgr_line_ch(&line.line, ' ');
            record_rational(&line.line, core_radius_place(orders->g[k], rho, length));
            record_line_ended(record, &line);
        }
    }
    core_radius_replaced(norms, &nf);
    core_radius_released(&nu);
    core_radius_released(&np);
    core_radius_released(&nw);
    return radius;
}

// The walls of a medium. With tau = T - t, X = r^2 / (2 nu tau) and the angular speed v = r tau^(-1-h) F(X, eta) of
// Proposition 20 of the millennium chapter on the coupled system, tau in seconds, v^2 = 2 nu X tau^(-1-2h) F^2. On
// [-1, 1] |T_m| <= 1 and |L^-1| <= l, and the proof on one ellipse gives ||f_k|| <= B_f r^-k past the split: F at X is
// at least F_low = c_0 - sum over m >= 1 of |c_m| less sum over k >= 1 of n_k (l^2 X)^k to the order the norms reach
// and B_f q^k past it, q = l^2 X / r < 1, the data F_0 = sum c_m T_m. For tau <= 1, tau^(-2h) >= 1 and the speed at X
// is at least V once tau <= 2 nu X F_low^2 / V^2: each wall below is a time left before T that the core passes it by.
// The speed V is a share of the sound speed, the light speed, or the speed sqrt(2 E / m) at which one particle of mass
// m carries the energy E that frees an electron; the radius r = sqrt(2 nu X tau) at X falls to the spacing a below
// which the medium is not a continuum at tau = a^2 / (2 nu X); and the medium answers as an elastic solid at tau = its
// relaxation time.

// F_low at X, from the data's weights, the norms to their order and the bound B_f past it
static SimRational core_radius_angular_least(const CoreRadiusSequence &data, const CoreRadiusSequence &norms,
                                             SimRational l, const CoreRadiusProof *proof, SimRational x)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    SimRational least = data.value[0];
    for (unsigned int m = 1u; m < data.count; m += 1u)
    {
        least = sim_rational_difference(least, sim_rational_absolute(data.value[m]));
    }
    const SimRational step = core_radius_times(core_radius_times(l, l), x);
    SimRational power = one;
    for (unsigned int k = 1u; k < norms.count; k += 1u)
    {
        power = core_radius_times(power, step);
        least = sim_rational_difference(least, core_radius_times(norms.value[k], power));
    }
    // B_f q^(N+1) / (1 - q), q = l^2 X / r
    const SimRational q = core_radius_over(step, proof->rate);
    SimRational tail = proof->b_f;
    for (unsigned int k = 0u; k < norms.count; k += 1u)
    {
        tail = core_radius_times(tail, q);
    }
    return sim_rational_difference(least, core_radius_over(tail, sim_rational_difference(one, q)));
}

static SimRational core_radius_least(SimRational left, SimRational right)
{
    return (sim_rational_sign(sim_rational_difference(left, right)) <= 0) ? left : right;
}

// the time left at which the speed at X reaches V, from V^2: 2 nu X F_low^2 / V^2, at max 1
static SimRational core_radius_wall_speed(SimRational nu, SimRational x, SimRational least, SimRational speed_square)
{
    const SimRational square = core_radius_times(core_radius_times(core_radius_times(core_radius_number(2ll, 1ll), nu), x), core_radius_times(least, least));
    return core_radius_least(core_radius_over(square, speed_square), core_radius_number(1ll, 1ll));
}

// the n with 10^-(n+1) < tau <= 10^-n, for 0 < tau <= 1
static unsigned int core_radius_decade(SimRational tau)
{
    const SimRational tenth = core_radius_number(1ll, 10ll);
    SimRational power = tenth;
    unsigned int decade = 0u;
    while (sim_rational_sign(sim_rational_difference(power, tau)) >= 0)
    {
        power = core_radius_times(power, tenth);
        decade += 1u;
    }
    return decade;
}

// the times the radius at X halves from tau to the Planck time: the largest n with tau >= 4^n t_P
static unsigned int core_radius_halves(SimRational tau, SimRational planck)
{
    const SimRational four = core_radius_number(4ll, 1ll);
    SimRational power = core_radius_times(planck, four);
    unsigned int halves = 0u;
    while (sim_rational_sign(sim_rational_difference(tau, power)) >= 0)
    {
        power = core_radius_times(power, four);
        halves += 1u;
    }
    return halves;
}

// The principal system at a point eta of the cut. Along a datum direction e^(mu eta), the paths that take one
// derivative of e^(mu eta) at every step sum, with Y = X / L^2 and the fields' numerators read at eta, to
//     2 (Y y')' = mu s y + mu L d - mu L d U' V,   V' = y,   2 (Y G)'' = mu s G + W (Y F)',   W = -mu L d V / Y,
// y the tangent's axial field, G its swirl, s = L (D eta + d U). On [0, Y_1] let s_eff = s - L d Y max(U', 0). Where
// s_eff >= s_least > 0: y and y' stay at least 0, V <= Y y, and 2 (Y y')' >= mu s_least y + mu L d, and comparison with
// z = (L d / s_least) (I_0(sqrt(2 s_least mu Y)) - 1), which solves it with equality, gives y >= z. Where (Y F)' is
// past 0 as well, W <= 0 drives G below 0, and -G grows as z does. The fields are read at the point from the exact
// orders, past them from the magnitudes carried on the device, and past those from the tail B r^-k of the proof.

// one field's series at the point: its exact coefficients, the magnitudes past them, and the proof's B and r past those
typedef struct
{
    CoreRadiusSequence exact;
    CoreRadiusSequence magnitudes;
    SimRational max;
    SimRational rate;
} CoreRadiusSeries;

enum
{
    CORE_RADIUS_VALUE = 0,
    CORE_RADIUS_SLOPE = 1,
    CORE_RADIUS_GROWN_SLOPE = 2
};

// the tail past `last` of sum c_k y^k (value), sum k c_k y^(k-1) (slope) or sum (k + 1) c_k y^k ((Y F)'), with
// |c_k| <= B r^-k, q = y / r < 1, K = last + 1
static SimRational core_radius_far_tail(unsigned int type, SimRational max, SimRational rate, SimRational y, unsigned int last)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational q = core_radius_over(y, rate);
    const SimRational rest = sim_rational_difference(one, q);
    const SimRational order_k = core_radius_number((long long)last + 1ll, 1ll);
    SimRational power = one;
    for (unsigned int k = 0u; k <= last; k += 1u)
    {
        power = core_radius_times(power, q);
    }
    // power = q^K
    if (type == CORE_RADIUS_VALUE)
    {
        return core_radius_over(core_radius_times(max, power), rest);
    }
    const SimRational square_rest = core_radius_times(rest, rest);
    if (type == CORE_RADIUS_SLOPE)
    {
        const SimRational before = core_radius_over(power, q);
        return core_radius_times(core_radius_over(max, rate),
                                 core_radius_plus(core_radius_over(core_radius_times(order_k, before), rest), core_radius_over(power, square_rest)));
    }
    return core_radius_times(max, core_radius_plus(core_radius_over(core_radius_times(core_radius_plus(order_k, one), power), rest),
                                                    core_radius_over(core_radius_times(power, q), square_rest)));
}

// a bound on [low, high], 0 <= low < high, of the series of `type`: the exact part at `low` moved by at max the width
// times the bound of its slope, and the rest at max its magnitudes and the tail at `high`; lower at sign -1, upper at +1
static SimRational core_radius_series_bound(const CoreRadiusSeries *series, unsigned int type, SimRational low, SimRational high, int sign)
{
    const SimRational zero = core_radius_number(0ll, 1ll);
    const SimRational one = core_radius_number(1ll, 1ll);
    SimRational at_low = zero;
    SimRational slope_max = zero;
    const unsigned int exact_last = series->exact.count - 1u;
    for (unsigned int k = 0u; k <= exact_last; k += 1u)
    {
        // the term a y^e: a = c_k, k c_k or (k + 1) c_k, and e = k, k - 1 or k
        const SimRational multiplier = (type == CORE_RADIUS_VALUE) ? one : core_radius_number((long long)k + ((type == CORE_RADIUS_GROWN_SLOPE) ? 1ll : 0ll), 1ll);
        const SimRational a = core_radius_times(multiplier, series->exact.value[k]);
        if ((type == CORE_RADIUS_SLOPE) && (k == 0u))
        {
            continue;
        }
        const unsigned int e = (type == CORE_RADIUS_SLOPE) ? k - 1u : k;
        SimRational low_power = one;
        SimRational high_power = one;
        for (unsigned int n = 0u; n < e; n += 1u)
        {
            low_power = core_radius_times(low_power, low);
        }
        for (unsigned int n = 1u; n < e; n += 1u)
        {
            high_power = core_radius_times(high_power, high);
        }
        at_low = core_radius_plus(at_low, core_radius_times(a, low_power));
        if (e >= 1u)
        {
            slope_max = core_radius_plus(slope_max, core_radius_times(core_radius_times(sim_rational_absolute(a), core_radius_number((long long)e, 1ll)), high_power));
        }
    }
    SimRational rest = zero;
    for (unsigned int index = 0u; index < series->magnitudes.count; index += 1u)
    {
        const unsigned int k = exact_last + 1u + index;
        const SimRational multiplier = (type == CORE_RADIUS_VALUE) ? one : core_radius_number((long long)k + ((type == CORE_RADIUS_GROWN_SLOPE) ? 1ll : 0ll), 1ll);
        const unsigned int e = (type == CORE_RADIUS_SLOPE) ? k - 1u : k;
        SimRational power = one;
        for (unsigned int n = 0u; n < e; n += 1u)
        {
            power = core_radius_times(power, high);
        }
        rest = core_radius_plus(
            rest, core_radius_times(core_radius_times(multiplier, series->magnitudes.value[index]), power));
    }
    const unsigned int last = exact_last + series->magnitudes.count;
    rest = core_radius_plus(rest, core_radius_far_tail(type, series->max, series->rate, high, last));
    const SimRational moved = core_radius_plus(core_radius_times(sim_rational_difference(high, low), slope_max), rest);
    return (sign < 0) ? sim_rational_difference(at_low, moved) : core_radius_plus(at_low, moved);
}

// the principal system at the vertex eta_v = a (rho0 + 1 / rho0) / 2 of E_rho0, where xi_v = (rho0 + 1 / rho0) / 2 and
// |T_m(xi_v)| <= rho0^m: the fields read at the vertex from the exact orders, past them from the magnitudes on E_rho0,
// and past those from the reached proof's tail. 1 where s_eff and (Y F)' are past 0 on [0, reach_y].
static int core_radius_principal(const CoreRadiusScale *scale, const CoreRadiusOrders *exact, const CoreRadiusOrders *reached, unsigned int order,
                                 unsigned int reach, const CoreRadiusProof *proof, SimRational reach_y, unsigned long long pieces, SimRational *vertex,
                                 SimRational *s_least, SimRational *grown_least)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational xi = core_radius_over(core_radius_plus(scale->rho, sim_rational_reciprocal(scale->rho)), two);
    *vertex = core_radius_times(scale->cut, xi);
    const SimRational eta_square = core_radius_times(*vertex, *vertex);
    const SimRational l_point = sim_rational_difference(one, core_radius_times(core_radius_times(two, scale->h), eta_square));
    const SimRational d_point = sim_rational_difference(one, eta_square);
    // each field's exact coefficients to order and its magnitudes past them to reach. They are cells of the temporary
    // tier, and the call gives them back before it returns
    CoreRadiusSeries along = {CORE_RADIUS_NONE, CORE_RADIUS_NONE, core_radius_number(0ll, 1ll),
                              core_radius_number(0ll, 1ll)};
    CoreRadiusSeries spin = {CORE_RADIUS_NONE, CORE_RADIUS_NONE, core_radius_number(0ll, 1ll),
                             core_radius_number(0ll, 1ll)};
    core_radius_taken(&along.exact, order + 1u, 1);
    core_radius_taken(&spin.exact, order + 1u, 1);
    core_radius_taken(&along.magnitudes, reach - order, 1);
    core_radius_taken(&spin.magnitudes, reach - order, 1);
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        core_radius_pushed(&along.exact, core_radius_value(&exact->g[k], xi));
        core_radius_pushed(&spin.exact, core_radius_value(&exact->f[k], xi));
    }
    for (unsigned int k = order + 1u; k <= reach; k += 1u)
    {
        core_radius_pushed(&along.magnitudes, core_radius_norm(&reached->g[k], scale->rho));
        core_radius_pushed(&spin.magnitudes, core_radius_norm(&reached->f[k], scale->rho));
    }
    along.max = proof->b_u;
    spin.max = proof->b_f;
    along.rate = proof->rate;
    spin.rate = proof->rate;
    *s_least = core_radius_number(0ll, 1ll);
    *grown_least = core_radius_number(0ll, 1ll);
    const int inside = (sim_rational_sign(sim_rational_difference(proof->rate, reach_y)) > 0) && (sim_rational_sign(l_point) > 0) &&
                       (sim_rational_sign(d_point) > 0);
    for (unsigned long long piece = 0ull; inside && (piece < pieces); piece += 1ull)
    {
        const SimRational low = core_radius_times(reach_y, core_radius_number((long long)piece, (long long)pieces));
        const SimRational high = core_radius_times(reach_y, core_radius_number((long long)piece + 1ll, (long long)pieces));
        const SimRational u_low = core_radius_series_bound(&along, CORE_RADIUS_VALUE, low, high, -1);
        const SimRational slope_high = core_radius_series_bound(&along, CORE_RADIUS_SLOPE, low, high, 1);
        const SimRational grown_low = core_radius_series_bound(&spin, CORE_RADIUS_GROWN_SLOPE, low, high, -1);
        // s_eff = L (D eta + d U) - L d Y max(U', 0), its least on the piece
        const SimRational s_low = core_radius_times(l_point, core_radius_plus(core_radius_times(scale->d, *vertex), core_radius_times(d_point, u_low)));
        const SimRational climb = core_radius_max(slope_high, core_radius_number(0ll, 1ll));
        const SimRational s_eff = sim_rational_difference(s_low, core_radius_times(core_radius_times(l_point, d_point), core_radius_times(high, climb)));
        *s_least = (piece == 0ull) ? s_eff : (sim_rational_sign(sim_rational_difference(s_eff, *s_least)) < 0 ? s_eff : *s_least);
        *grown_least = (piece == 0ull) ? grown_low : (sim_rational_sign(sim_rational_difference(grown_low, *grown_least)) < 0 ? grown_low : *grown_least);
    }
    core_radius_released(&along.exact);
    core_radius_released(&spin.exact);
    core_radius_released(&along.magnitudes);
    core_radius_released(&spin.magnitudes);
    return inside && (sim_rational_sign(*s_least) > 0) && (sim_rational_sign(*grown_least) > 0);
}

// the bytes of a wall's name, its terminator among them: a literal and at most one rational
#define CORE_RADIUS_WALL_NAME (RECORD_RATIONAL_ROOM + 64u)

// one wall: its name and the time left the core passes it by
typedef struct
{
    char name[CORE_RADIUS_WALL_NAME];
    SimRational tau;
} CoreRadiusWall;

// the wall "<before><share><after>" of the time left `tau`, the share left out where `share` is NULL
static CoreRadiusWall core_radius_wall(const char *before, const SimRational *share, const char *after, SimRational tau)
{
    CoreRadiusWall wall;
    VerbaLinea name = {wall.name, sizeof(wall.name), 0ull};
    mmgr_line_put(&name, before);
    if (share != NULL)
    {
        record_rational(&name, *share);
    }
    mmgr_line_put(&name, after);
    (void)mmgr_line_finish(&name);
    wall.tau = tau;
    return wall;
}

// the order of two walls, the one with more time left first: below 0, 0 or above 0
static int core_radius_wall_order(const void *left, const void *right)
{
    const CoreRadiusWall *const one = (const CoreRadiusWall *)left;
    const CoreRadiusWall *const other = (const CoreRadiusWall *)right;
    return sim_rational_sign(sim_rational_difference(other->tau, one->tau));
}

// the walls of one medium in `walls`, and their count. The buffer `walls` has room for the mach shares and 4 more. The
// wall with the max time left comes first, and walls of the same time left keep the order they come in. `room` has
// the room of `walls`, and the sort writes it
static unsigned int core_radius_walls(SimRational nu, SimRational x, SimRational least, SimRational sound,
                                      SimRational spacing, SimRational relaxation, SimRational mass,
                                      SimRational ionization, SimRational light, const CoreRadiusSequence &mach,
                                      CoreRadiusWall *walls, CoreRadiusWall *room)
{
    unsigned int count = 0u;
    for (unsigned int index = 0u; index < mach.count; index += 1u)
    {
        const SimRational speed = core_radius_times(mach.value[index], sound);
        walls[count] = core_radius_wall("speed ", &mach.value[index], " of sound",
                                        core_radius_wall_speed(nu, x, least, core_radius_times(speed, speed)));
        count += 1u;
    }
    if (sim_rational_sign(mass) > 0)
    {
        // V^2 = 2 E / m
        walls[count] = core_radius_wall(
            "collisions that free an electron", NULL, "",
            core_radius_wall_speed(
                nu, x, least, core_radius_over(core_radius_times(core_radius_number(2ll, 1ll), ionization), mass)));
        count += 1u;
    }
    walls[count] = core_radius_wall("speed of light", NULL, "",
                                    core_radius_wall_speed(nu, x, least, core_radius_times(light, light)));
    count += 1u;
    const SimRational radius_tau = core_radius_over(core_radius_times(spacing, spacing), core_radius_times(core_radius_times(core_radius_number(2ll, 1ll), nu), x));
    const SimRational one = core_radius_number(1ll, 1ll);
    walls[count] = core_radius_wall("radius at the spacing", NULL, "", core_radius_least(radius_tau, one));
    count += 1u;
    if (sim_rational_sign(relaxation) > 0)
    {
        walls[count] = core_radius_wall("elastic answer", NULL, "", core_radius_least(relaxation, one));
        count += 1u;
    }
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = walls, .bytes = sizeof(CoreRadiusWall), .scratch = room,
                     .count = count, .order = core_radius_wall_order);
    return count;
}

// the largest r = j step, j = 1..steps, the proof is valid at over the witness a, u, p, w, 0 where it is valid at none,
// with the witness and r written to the record under `name`
static SimRational core_radius_rate(const CoreRadiusScale *scale, const CoreRadiusSequence &a,
                                    const CoreRadiusSequence &u, const CoreRadiusSequence &p,
                                    const CoreRadiusSequence &w, unsigned int order, SimRational step,
                                    unsigned long long steps, FILE *record, const char *name)
{
    // every B_x grows with r and 1 / r falls: where the proof is valid at r it is valid below r, and j is bisected
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
        PlatformText line;
        record_line_begun(&line);
        mmgr_line_put(&line.line, "  ");
        mmgr_line_put(&line.line, name);
        mmgr_line_put(&line.line, ", tail E ");
        record_rational(&line.line, scale->feet);
        mmgr_line_put(&line.line, ": a, u, p, w to order ");
        mmgr_line_decimal(&line.line, order, 1u);
        record_line_ended(record, &line);
        for (unsigned int k = 0u; k <= order; k += 1u)
        {
            core_radius_four_recorded(record, a.value[k], u.value[k], p.value[k], w.value[k]);
        }
        core_radius_proved_recorded(record, proved, core_radius_times(proved, scale->delta));
    }
    return proved;
}

// the media a cfg names at max, and the bytes of a medium's name, its terminator among them
#define CORE_RADIUS_MEDIA 64u
#define CORE_RADIUS_MEDIUM_NAME 256u

// the name of the witness `<ellipse><kind>` into `name`, which has room for CORE_RADIUS_WITNESS_NAME
#define CORE_RADIUS_WITNESS_NAME 64u
static void core_radius_witness_name(char *name, const char *ellipse, const char *kind)
{
    VerbaLinea line = {name, CORE_RADIUS_WITNESS_NAME, 0ull};
    mmgr_line_put(&line, ellipse);
    mmgr_line_put(&line, kind);
    (void)mmgr_line_finish(&line);
}

// the bytes of a witness's title, its terminator among them: literals and at most two rationals
#define CORE_RADIUS_TITLE ((2u * RECORD_RATIONAL_ROOM) + 256u)

// the title "On E_rho0, rho0 = <rho0>[ in xi = eta / <cut>], <kind><order>." into `title`. The buffer `title` has
// room for CORE_RADIUS_TITLE. The title names the cut where the ellipse is not whole
static void core_radius_title(char *title, SimRational rho0, int whole, SimRational cut, const char *kind,
                              unsigned long long order)
{
    VerbaLinea line = {title, CORE_RADIUS_TITLE, 0ull};
    mmgr_line_put(&line, "On E_rho0, rho0 = ");
    record_rational(&line, rho0);
    if (!whole)
    {
        mmgr_line_put(&line, " in xi = eta / ");
        record_rational(&line, cut);
    }
    mmgr_line_put(&line, ", ");
    mmgr_line_put(&line, kind);
    mmgr_line_decimal(&line, order, 1u);
    mmgr_line_ch(&line, '.');
    (void)mmgr_line_finish(&line);
}

int program_main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    SimRational cut;
    // the cfg's lists, each in a cell of the cfg's (run_cfg_rationals)
    CoreRadiusSequence angular = CORE_RADIUS_NONE;
    CoreRadiusSequence axial = CORE_RADIUS_NONE;
    CoreRadiusSequence pressure = CORE_RADIUS_NONE;
    CoreRadiusSequence outer = CORE_RADIUS_NONE;
    CoreRadiusSequence inner = CORE_RADIUS_NONE;
    unsigned long long order = 0ull;
    unsigned long long steps = 0ull;
    unsigned long long split = 0ull;
    unsigned long long carry = 0ull;
    unsigned long long reach = 0ull;
    unsigned long long reach_split = 0ull;
    unsigned long long bits = 0ull;
    SimRational step;
    SimRational wall_step;
    SimRational light;
    SimRational planck;
    CoreRadiusSequence mach = CORE_RADIUS_NONE;
    // the names' text and the 7 bytes past its room that a run of cellul reads at max
    char names_text[RUN_CFG_TEXT_ROOM + 8u] = {0};
    CoreRadiusSequence viscosity = CORE_RADIUS_NONE;
    CoreRadiusSequence sound = CORE_RADIUS_NONE;
    CoreRadiusSequence spacing = CORE_RADIUS_NONE;
    CoreRadiusSequence relaxation = CORE_RADIUS_NONE;
    CoreRadiusSequence mass = CORE_RADIUS_NONE;
    CoreRadiusSequence ionization = CORE_RADIUS_NONE;
    const int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
        run_cfg_rational(&cfg, "core.anisotropy", &h) &&
        run_cfg_rationals(&cfg, "core.axis.angular", &angular.value, &angular.count) &&
        run_cfg_rationals(&cfg, "core.axis.axial", &axial.value, &axial.count) &&
        run_cfg_rationals(&cfg, "core.axis.pressure", &pressure.value, &pressure.count) &&
        run_cfg_rationals(&cfg, "ellipse.outer", &outer.value, &outer.count) &&
        run_cfg_rationals(&cfg, "ellipse.inner", &inner.value, &inner.count) && (outer.count == inner.count) &&
        run_cfg_rational(&cfg, "ellipse.cut", &cut) && run_cfg_count(&cfg, "order", &order) && (order >= 1ull) &&
        run_cfg_rational(&cfg, "rate.step", &step) && (sim_rational_sign(step) > 0) &&
        run_cfg_count(&cfg, "rate.steps", &steps) && (steps >= 1ull) && run_cfg_count(&cfg, "split", &split) &&
        (split >= 1ull) && (order >= 2ull * split) && run_cfg_count(&cfg, "carry", &carry) && (carry >= order) &&
        run_cfg_count(&cfg, "reach.order", &reach) && (reach >= carry) &&
        run_cfg_count(&cfg, "reach.split", &reach_split) && (reach_split >= 1ull) && (reach >= 2ull * reach_split) &&
        run_cfg_count(&cfg, "bits", &bits) && (bits >= 1ull) && run_cfg_rational(&cfg, "walls.step", &wall_step) &&
        (sim_rational_sign(wall_step) > 0) && run_cfg_rational(&cfg, "walls.light", &light) &&
        run_cfg_rational(&cfg, "walls.planck", &planck) && (sim_rational_sign(planck) > 0) &&
        run_cfg_rationals(&cfg, "walls.mach", &mach.value, &mach.count) &&
        run_cfg_text(&cfg, "walls.names", names_text, RUN_CFG_TEXT_ROOM) &&
        run_cfg_rationals(&cfg, "walls.viscosity", &viscosity.value, &viscosity.count) &&
        run_cfg_rationals(&cfg, "walls.sound", &sound.value, &sound.count) &&
        run_cfg_rationals(&cfg, "walls.spacing", &spacing.value, &spacing.count) &&
        run_cfg_rationals(&cfg, "walls.relaxation", &relaxation.value, &relaxation.count) &&
        run_cfg_rationals(&cfg, "walls.mass", &mass.value, &mass.count) &&
        run_cfg_rationals(&cfg, "walls.ionization", &ionization.value, &ionization.count);
    CoreRadiusSequence *const lists[12] = {&angular,   &axial, &pressure, &outer,      &inner, &mach,
                                           &viscosity, &sound, &spacing,  &relaxation, &mass,  &ionization};
    for (unsigned int list = 0u; list < 12u; list += 1u)
    {
        lists[list]->room = lists[list]->count;
    }
    // the principal system, read where the cfg contains it
    SimRational principal_reach;
    unsigned long long principal_pieces = 0ull;
    const int principal = read && run_cfg_rational(&cfg, "principal.reach", &principal_reach) &&
                          (sim_rational_sign(principal_reach) > 0) && run_cfg_count(&cfg, "principal.pieces", &principal_pieces) && (principal_pieces >= 1ull);
    // the media's names, one to each comma, its spaces before it skipped, each a record of a fixed bound
    static char s_media[CORE_RADIUS_MEDIA][CORE_RADIUS_MEDIUM_NAME];
    size_t count_media = 0u;
    int media_fit = 1;
    const size_t names_size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = names_text, .cap = RUN_CFG_TEXT_ROOM);
    size_t from = 0u;
    while (read && media_fit && (from <= names_size))
    {
        const size_t end = from + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = names_text + from,
                                             .cap = names_size - from, .other = ",");
        const size_t first = from + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = names_text + from,
                                               .cap = names_size - from, .other = " ");
        const size_t length = (first >= end) ? 0u : (end - first);
        media_fit = (count_media < CORE_RADIUS_MEDIA) && (length < CORE_RADIUS_MEDIUM_NAME);
        if (media_fit)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_media[count_media], .src = names_text + first, .bytes = length);
            s_media[count_media][length] = '\0';
            count_media += 1u;
        }
        from = end + 1u;
    }
    if (!media_fit)
    {
        mmgr_line_put(
            &results.line,
            "  walls.names: more than CORE_RADIUS_MEDIA media, or a name of CORE_RADIUS_MEDIUM_NAME bytes or more\n");
    }
    const int media_read = read && media_fit && (viscosity.count == count_media) && (sound.count == count_media) &&
                           (spacing.count == count_media) && (relaxation.count == count_media) &&
                           (mass.count == count_media) && (ionization.count == count_media);
    if (!media_read || (count_media == 0u))
    {
        run_cfg_missing(&results.line, "core anisotropy, axis angular, axial and pressure, ellipse outer and inner of one length and a cut, order, rate "
                                       "step and steps, a split at least 1 with order at least twice it, a carry at least the order, a reach order at least the carry and twice its split, bits, and walls "
                                       "step, light, planck, mach, and names with one viscosity, sound, spacing, relaxation, mass and ionization each");
        sim_flush(&results);
        fprintf(stderr, "core_radius <cfg>\n");
        return 2;
    }
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational half = core_radius_number(1ll, 2ll);
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int legal = (sim_rational_sign(h) >= 0) && (sim_rational_sign(sim_rational_difference(half, h)) >= 0);
    // the cut: 0 < a <= 1, the whole segment at a = 1
    legal = legal && (sim_rational_sign(cut) > 0) && (sim_rational_sign(sim_rational_difference(one, cut)) >= 0);
    const int whole = sim_rational_sign(sim_rational_difference(one, cut)) == 0;
    CoreRadiusScale constants;
    constants.h = h;
    constants.cut = cut;
    constants.d = sim_rational_difference(half, h);
    constants.a = core_radius_plus(half, h);
    CoreRadiusOrders orders;
    core_radius_weights(&constants, &angular, &axial, &pressure, (unsigned int)order, one, &orders);
    CoreRadiusOrders exact;
    core_radius_weights(&constants, &angular, &axial, &pressure, (unsigned int)order, core_radius_number(-1ll, 1ll),
                        &exact);
    // g0, the data's degree, and every order's weights within the length G_k + 1 the rule fixes, w_k within G_k + 4
    const unsigned int widest_data = (angular.count > axial.count) ? angular.count : axial.count;
    const unsigned int g0 = ((widest_data > pressure.count) ? widest_data : pressure.count) - 1u;
    CoreRadiusOrders continued;
    // the carried magnitudes rounded up to multiples of 2^-bits
    OriorExactInteger unit;
    sim_rational_status_check(sim_exact_power(2ull, bits, &unit));
    core_radius_continued(&constants, &exact, (unsigned int)order, (unsigned int)carry, &unit, NULL, &continued);
    // the carry on the device to the cfg's reach, its primes enough for sums of twice the bits and 256 more
    CoreRadiusDevice device;
    const unsigned int reach_modes = g0 + (g0 + 4u) * (unsigned int)reach + 8u;
    core_radius_device_open(&device, (unsigned int)reach + 1u, reach_modes, (unsigned int)((2ull * bits + 256ull) / 30ull + 1ull));
    CoreRadiusOrders reached;
    core_radius_continued(&constants, &exact, (unsigned int)order, (unsigned int)reach, &unit, &device, &reached);
    sim_check(&results, device.available, "the device's carry bounded, every sum below the product of its primes");
    core_radius_device_close(&device);
    int same = device.available;
    for (unsigned int k = 0u; same && (k <= (unsigned int)carry); k += 1u)
    {
        same = core_radius_same(&continued.f[k], &reached.f[k]) && core_radius_same(&continued.g[k], &reached.g[k]) &&
               core_radius_same(&continued.q[k], &reached.q[k]) &&
               core_radius_same(&continued.inflow[k], &reached.inflow[k]);
    }
    sim_check(&results, same, "the device's carry is the host's to the last bit at every order both reach");
    int fixed_length = 1;
    for (unsigned int k = 0u; k <= (unsigned int)reach; k += 1u)
    {
        const size_t length = (size_t)g0 + (size_t)(g0 + 4u) * k + 1u;
        fixed_length = fixed_length && (reached.f[k].count <= length) && (reached.g[k].count <= length) &&
                       (reached.q[k].count <= length) && (reached.inflow[k].count <= length + 3u);
    }
    for (unsigned int k = 0u; k <= (unsigned int)carry; k += 1u)
    {
        const size_t length = (size_t)g0 + (size_t)(g0 + 4u) * k + 1u;
        fixed_length = fixed_length && (continued.f[k].count <= length) && (continued.g[k].count <= length) &&
                       (continued.q[k].count <= length) && (continued.inflow[k].count <= length + 3u);
    }
    sim_check(&results, fixed_length, "every order's weights within the length the rule fixes");
    // the cfg's name, past the last slash or backslash of its path
    const char *cfg_name = arguments[1];
    const char *const slash = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = arguments[1],
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const backslash = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = arguments[1],
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    cfg_name = ((slash != NULL) && ((slash + 1) > cfg_name)) ? (slash + 1) : cfg_name;
    cfg_name = ((backslash != NULL) && ((backslash + 1) > cfg_name)) ? (backslash + 1) : cfg_name;
    PlatformText lean;
    core_radius_lean_begun(&lean);
    mmgr_line_put(&lean.line,
                  "-- SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n"
                  "import CoreRadius.Witness\n\n"
                  "/-!\n"
                  "# The numbers each ellipse proves\n\n"
                  "`core_radius.cu` writes this file from `cfg/");
    mmgr_line_put(&lean.line, cfg_name);
    mmgr_line_put(&lean.line, "`: for each ellipse, the numbers the proof on it reads at\n"
                              "the largest r it proves, with each weight exact and with the magnitudes carried, and "
                              "the theorem that they pass\n"
                              "`Witness.Checks`. `Witness.tail` carries them to every order past the split.\n"
                              "-/\n\n"
                              "namespace CoreRadius\n\n");
    int principal_all = 1;
    int found = 0;
    SimRational best_x = core_radius_number(0ll, 1ll);
    SimRational best_least = core_radius_number(0ll, 1ll);
    SimRational best_value = core_radius_number(0ll, 1ll);
    SimRational best_ellipse = core_radius_number(0ll, 1ll);
    for (unsigned int index = 0u; index < outer.count; index += 1u)
    {
        const SimRational rho_square = core_radius_times(outer.value[index], outer.value[index]);
        // L = (1 - h a^2) - h a^2 T_2 is at least (1 - h a^2) - h a^2 rho0^2 on E_rho0
        const SimRational h_square = core_radius_times(core_radius_times(h, cut), cut);
        const SimRational l_inverse = sim_rational_difference(sim_rational_difference(one, h_square), core_radius_times(h_square, rho_square));
        const SimRational inner_square =
            sim_rational_difference(core_radius_times(inner.value[index], inner.value[index]), one);
        CoreRadiusScale scale;
        scale.h = h;
        scale.cut = cut;
        scale.rho = outer.value[index];
        scale.delta = sim_rational_difference(outer.value[index], inner.value[index]);
        const int conditions_met = (sim_rational_sign(scale.delta) > 0) && (sim_rational_sign(l_inverse) > 0) &&
                                   (sim_rational_sign(inner_square) > 0);
        legal = legal && conditions_met;
        if (!conditions_met)
        {
            continue;
        }
        scale.l = sim_rational_reciprocal(l_inverse);
        scale.k3 = core_radius_over(core_radius_times(core_radius_number(3ll, 1ll), rho_square), inner_square);
        scale.d = sim_rational_difference(half, h);
        scale.a = core_radius_plus(half, h);
        if (record != NULL)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "ellipse rho0 ");
            record_rational(&line.line, outer.value[index]);
            mmgr_line_put(&line.line, ", rho_min ");
            record_rational(&line.line, inner.value[index]);
            record_line_ended(record, &line);
        }
        // E as (1 + rho0^2) / 2 times 3 K, then as the feet's 3 (1 + rho0^2) / 4
        const unsigned int last = (unsigned int)order;
        CoreRadiusSequence a = CORE_RADIUS_NONE;
        CoreRadiusSequence u = CORE_RADIUS_NONE;
        CoreRadiusSequence p = CORE_RADIUS_NONE;
        CoreRadiusSequence w = CORE_RADIUS_NONE;
        // the four witnesses read on the whole segment alone: their constants bound the feet of d at eta = -1 and +1
        SimRational plain = core_radius_number(0ll, 1ll);
        SimRational feet = core_radius_number(0ll, 1ll);
        SimRational weights = core_radius_number(0ll, 1ll);
        SimRational retained = core_radius_number(0ll, 1ll);
        if (whole)
        {
            scale.feet = core_radius_times(core_radius_over(core_radius_plus(one, rho_square), core_radius_number(2ll, 1ll)), scale.k3);
            core_radius_witness(&scale, core_radius_norm(&angular, scale.rho), core_radius_norm(&axial, scale.rho),
                                core_radius_norm(&pressure, scale.rho), last, &a, &u, &p, &w);
            plain = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "bounds, d f' by K");
            scale.feet = core_radius_over(core_radius_times(core_radius_number(3ll, 1ll), core_radius_plus(one, rho_square)), core_radius_number(4ll, 1ll));
            core_radius_witness(&scale, core_radius_norm(&angular, scale.rho), core_radius_norm(&axial, scale.rho),
                                core_radius_norm(&pressure, scale.rho), last, &a, &u, &p, &w);
            feet = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "bounds, d f' at the feet");
            core_radius_weights_witness(&scale, &orders, last, &a, &u, &p, &w);
            weights = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "weights");
            core_radius_weights_witness(&scale, &exact, last, &a, &u, &p, &w);
            retained = core_radius_rate(&scale, a, u, p, w, last, step, steps, record, "exact weights");
        }
        // a cut's witnesses carry names of their own, beside the whole segment's in one Lean library
        char ellipse[CORE_RADIUS_WITNESS_NAME];
        VerbaLinea ellipse_line = {ellipse, sizeof(ellipse), 0ull};
        mmgr_line_put(&ellipse_line, whole ? "ellipse_" : "cut_ellipse_");
        mmgr_line_decimal(&ellipse_line, index, 1u);
        (void)mmgr_line_finish(&ellipse_line);
        static char s_title[CORE_RADIUS_TITLE];
        char witness[CORE_RADIUS_WITNESS_NAME];
        CoreRadiusProof exact_proof = {};
        CoreRadiusSequence exact_norms = CORE_RADIUS_NONE;
        core_radius_witness_name(witness, ellipse, "_exact");
        core_radius_title(s_title, outer.value[index], whole, cut, "each weight exact to order ", last);
        const SimRational one_ellipse =
            core_radius_fixed_rate(&scale, &exact, g0, last, (unsigned int)split, step, steps, record, witness, s_title,
                                   &lean, &exact_proof, &exact_norms);
        CoreRadiusProof carried_proof = {};
        CoreRadiusSequence carried_norms = CORE_RADIUS_NONE;
        core_radius_witness_name(witness, ellipse, "_carried");
        core_radius_title(s_title, outer.value[index], whole, cut, "the magnitudes carried to order ", carry);
        const SimRational carried =
            core_radius_fixed_rate(&scale, &continued, g0, (unsigned int)carry, (unsigned int)split, step, steps,
                                   record, witness, s_title, &lean, &carried_proof, &carried_norms);
        CoreRadiusProof reached_proof = {};
        CoreRadiusSequence reached_norms = CORE_RADIUS_NONE;
        core_radius_witness_name(witness, ellipse, "_reached");
        core_radius_title(s_title, outer.value[index], whole, cut, "the magnitudes carried on the device to order ",
                          reach);
        const SimRational far =
            core_radius_fixed_rate(&scale, &reached, g0, (unsigned int)reach, (unsigned int)reach_split, step, steps,
                                   record, witness, s_title, &lean, &reached_proof, &reached_norms);
        if (principal)
        {
            SimRational vertex;
            SimRational s_least;
            SimRational grown_least;
            const int vertex_ok = (sim_rational_sign(reached_proof.rate) > 0) &&
                                  core_radius_principal(&scale, &exact, &reached, (unsigned int)order,
                                                        (unsigned int)reach, &reached_proof, principal_reach,
                                                        principal_pieces, &vertex, &s_least, &grown_least);
            principal_all = principal_all && vertex_ok;
            if (record != NULL)
            {
                PlatformText line;
                record_line_begun(&line);
                mmgr_line_put(&line.line, "principal system at the vertex eta ");
                record_rational(&line.line, vertex);
                mmgr_line_put(&line.line, " on Y in [0, ");
                record_rational(&line.line, principal_reach);
                mmgr_line_put(&line.line, "], Y = X / L^2: s_eff >= ");
                record_rational(&line.line, s_least);
                mmgr_line_put(&line.line, ", (Y F)' >= ");
                record_rational(&line.line, grown_least);
                record_line_ended(record, &line);
            }
            mmgr_line_put(&results.line, "  principal system at the vertex eta ");
            sim_rational_print(&results.line, core_radius_down(vertex, &unit));
            mmgr_line_put(&results.line, ": s_eff >= ");
            sim_rational_print(&results.line, core_radius_down(s_least, &unit));
            mmgr_line_put(&results.line, ", (Y F)' >= ");
            sim_rational_print(&results.line, core_radius_down(grown_least, &unit));
            mmgr_line_put(&results.line, "\n");
            sim_flush(&results);
        }
        // the X of each proof's radius, stepped by the walls' step, where X F_low^2 is largest
        const CoreRadiusProof *const proofs[3] = {&exact_proof, &carried_proof, &reached_proof};
        const CoreRadiusSequence *const proof_norms[3] = {&exact_norms, &carried_norms, &reached_norms};
        const SimRational radii[3] = {one_ellipse, carried, far};
        for (unsigned int which = 0u; which < 3u; which += 1u)
        {
            if (sim_rational_sign(proofs[which]->rate) <= 0)
            {
                continue;
            }
            SimRational x = wall_step;
            while (sim_rational_sign(sim_rational_difference(radii[which], x)) > 0)
            {
                const SimRational exact_least = core_radius_angular_least(angular, *proof_norms[which], scale.l, proofs[which], x);
                if (sim_rational_sign(exact_least) <= 0)
                {
                    x = core_radius_plus(x, wall_step);
                    continue;
                }
                // rounded down to a multiple of 2^-bits, stored in a length the walls' products fit
                const SimRational least = core_radius_down(exact_least, &unit);
                const SimRational value = core_radius_times(x, core_radius_times(least, least));
                if ((sim_rational_sign(least) > 0) && (!found || (sim_rational_sign(sim_rational_difference(value, best_value)) > 0)))
                {
                    found = 1;
                    best_x = x;
                    best_least = least;
                    best_value = value;
                    best_ellipse = outer.value[index];
                }
                x = core_radius_plus(x, wall_step);
            }
        }
        mmgr_line_put(&results.line, "  rho0 ");
        sim_rational_print(&results.line, outer.value[index]);
        mmgr_line_put(&results.line, ", rho_min ");
        sim_rational_print(&results.line, inner.value[index]);
        mmgr_line_put(&results.line, ": X < ");
        sim_rational_print(&results.line, core_radius_times(plain, scale.delta));
        mmgr_line_put(&results.line, " with d f' bounded by K, X < ");
        sim_rational_print(&results.line, core_radius_times(feet, scale.delta));
        mmgr_line_put(&results.line, " with it bounded at the feet, X < ");
        sim_rational_print(&results.line, core_radius_times(weights, scale.delta));
        mmgr_line_put(&results.line, " with each mode's weight retained, X < ");
        sim_rational_print(&results.line, core_radius_times(retained, scale.delta));
        mmgr_line_put(&results.line, " with each weight exact, X < ");
        sim_rational_print(&results.line, one_ellipse);
        mmgr_line_put(&results.line, " on E_rho0 alone, X < ");
        sim_rational_print(&results.line, carried);
        mmgr_line_put(&results.line, " with the magnitudes carried to the order of the cfg's carry, X < ");
        sim_rational_print(&results.line, far);
        mmgr_line_put(&results.line, " with them carried on the device to the cfg's reach and split there\n");
        sim_flush(&results);
        CoreRadiusSequence *const sequences[7] = {&a, &u, &p, &w, &exact_norms, &carried_norms, &reached_norms};
        for (unsigned int each = 0u; each < 7u; each += 1u)
        {
            core_radius_released(sequences[each]);
        }
        core_radius_proof_released(&exact_proof);
        core_radius_proof_released(&carried_proof);
        core_radius_proof_released(&reached_proof);
    }
    sim_check(&results, legal, "every ellipse of the cfg one the bounds are valid on");
    if (principal)
    {
        sim_check(&results, principal_all, "at every ellipse's vertex, s_eff and (Y F)' past 0 on [0, Y_1]: the principal tangent grows as I_0(sqrt(2 s_least mu Y))");
    }
    sim_check(&results, found, "an X inside a proved radius where F_low is past 0");
    if (found)
    {
        PlatformText line;
        record_line_begun(&line);
        mmgr_line_put(&line.line, "walls at X = ");
        record_rational(&line.line, best_x);
        mmgr_line_put(&line.line, " on E_rho0, rho0 = ");
        record_rational(&line.line, best_ellipse);
        mmgr_line_put(&line.line, ", F_low = ");
        record_rational(&line.line, best_least);
        const char *const at = mmgr_line_ok(&line.line) ? platform_text_bytes(&line) : NULL;
        mmgr_line_put(&results.line, "  ");
        mmgr_line_put(&results.line, (at != NULL) ? at : "walls at X: the line passed RECORD_LINE_MAX");
        mmgr_line_ch(&results.line, '\n');
        if (record != NULL)
        {
            record_line_ended(record, &line);
        }
        else
        {
            platform_text_ended(&line);
        }
        // the walls of a medium and the room their sort writes, cells of the temporary tier at the walls' max
        CoreRadiusWall *const walls = (CoreRadiusWall *)platform_cell_taken(
            2ull * ((unsigned long long)mach.count + 4ull) * sizeof(CoreRadiusWall), platform_place_g_program_owner, 1);
        for (unsigned int index = 0u; (walls != NULL) && (index < count_media); index += 1u)
        {
            const unsigned int walls_count =
                core_radius_walls(viscosity.value[index], best_x, best_least, sound.value[index], spacing.value[index],
                                  relaxation.value[index], mass.value[index], ionization.value[index], light, mach,
                                  walls, walls + mach.count + 4u);
            mmgr_line_put(&results.line, "  ");
            mmgr_line_put(&results.line, s_media[index]);
            mmgr_line_put(&results.line, ", the time left before T each wall is passed by:\n");
            if (record != NULL)
            {
                record_line_begun(&line);
                mmgr_line_put(&line.line, "  ");
                mmgr_line_put(&line.line, s_media[index]);
                mmgr_line_put(&line.line, ": wall, time left in seconds, n with 10^-(n+1) < tau <= 10^-n, halves of "
                                          "the radius to the Planck time");
                record_line_ended(record, &line);
            }
            for (unsigned int at = 0u; at < walls_count; at += 1u)
            {
                const CoreRadiusWall &wall = walls[at];
                const unsigned int decade = core_radius_decade(wall.tau);
                const unsigned int halves = core_radius_halves(wall.tau, planck);
                mmgr_line_put(&results.line, "    ");
                mmgr_line_put(&results.line, wall.name);
                mmgr_line_put(&results.line, ": 10^-");
                mmgr_line_decimal(&results.line, decade + 1u, 1u);
                mmgr_line_put(&results.line, " < tau <= 10^-");
                mmgr_line_decimal(&results.line, decade, 1u);
                mmgr_line_put(&results.line, " s, then ");
                mmgr_line_decimal(&results.line, halves, 1u);
                mmgr_line_put(&results.line, " halves of the radius to the Planck time\n");
                if (record != NULL)
                {
                    record_line_begun(&line);
                    mmgr_line_put(&line.line, "    ");
                    mmgr_line_put(&line.line, wall.name);
                    mmgr_line_put(&line.line, ", ");
                    record_rational(&line.line, wall.tau);
                    mmgr_line_put(&line.line, ", ");
                    mmgr_line_decimal(&line.line, decade, 1u);
                    mmgr_line_put(&line.line, ", ");
                    mmgr_line_decimal(&line.line, halves, 1u);
                    record_line_ended(record, &line);
                }
            }
            sim_flush(&results);
        }
        platform_cell_released(walls, platform_place_g_program_owner);
    }
    mmgr_line_put(&lean.line, "end CoreRadius\n");
    sim_check(&results, core_radius_lean_write(arguments[1], &cfg, "report.lean", &lean), "Lean witness written");
    platform_text_ended(&lean);
    core_radius_orders_released(&orders);
    core_radius_orders_released(&exact);
    core_radius_orders_released(&continued);
    core_radius_orders_released(&reached);
    if (core_radius_unmet())
    {
        mmgr_line_put(&results.line,
                      "  the pool did not meet a cell of the weights: run with a larger g_program_pool_bytes\n");
    }
    const int conditions_met =
        !run_cfg_short() && !record_short() && (g_sim_rational_wide == 0) && !core_radius_unmet();
    mmgr_line_put(&results.line, conditions_met
                                      ? "  every bound is exact and stored in the build's width\n"
                                      : "  a bound outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, conditions_met, "every bound bounded");
    const int recorded = (record != NULL) && record_close(record);
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core radius");
}
