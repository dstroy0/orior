// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching_rule.cu: the exterior, its tails and the parts of the six matching functions of Duraiswami's matched core
// as identities among stored functions (jet_rule.h)
#include "run_cfg.h"

#include "jet_rule.h"
#include "record.h"
#include "../../../../includes/platform/platform.h"

// With A = 1/2 + h, D = 1/2 - h, d = 1 - eta^2 and L = 1 - 2 h eta^2, the paper's left sides are
//     R_theta = T_{-(A+1/2)} F + v0 (X F_X + F) + U Z_{-(A+1/2)} F - 2 (X F)_XX,
//     R_z = T_{-A} U + X v0 U_X + U Z_{-A} U + Z_{-2A} Pi - 2 (X U_X)_X,
//     T_b f = L^-1 (-b f + D eta f_eta + X f_X),   Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X),
// and its exterior F_ext = (c / sqrt 2) (2d)^(-1-h) w(z), z = X / (2d), w = U(1 + h, 2, z). The torque is
// int X R_theta and the force int R_z over the annulus; V = X v0 and Pi are taken out by parts through
// V_X = C = L^-1 (2 A eta U - d U_eta + 2 eta X U_X) and Pi_X = F^2:
//     int X R_theta = [V X F - 2 X^2 F_X] + int (X T F + X U Z F - C X F),
//     int R_z = [V U - 2 X U_X + L^-1 (2 b eta X Pi + d X Pi_eta)]
//               + int (T U + U Z U - C U - L^-1 (2 b eta X F^2 + 2 d X F F_eta + 2 eta X F^2)),
// b = -2A in the bracket. An identity int g = [G] + int q over every interval is g = G_X + q, and F and U are any
// functions of X and eta. The tails of H and S are
//     int (z w - z^(-h)) dz = (z^2 (w' - w) + z^(1-h)) / (h - 1),
//     int X F_ext^2 dX = -c^2 X^(-2h) / (4 h) + (c^2 / 2) (2d)^(-2h) int (z w^2 - z^(-1-2h)) dz,
// with H = 2 X F_ext and H_pow = sqrt 2 c X^(-h), int H_pow dX = sqrt 2 c X^(1-h) / (1 - h).
// Checks:
// 1. The torque's and the force's parts: g - G_X - q is 0 for any F and U, V and Pi defined by their relations.
// 2. The same with one part of the torque's changed is not 0.
// 3. F_ext with Kummer's w solves the exterior's equation T_{-(A+1/2)} F - 2 (X F)_XX = 0.
// 4. The tails: each identity's derivative in X is 0, with Kummer's equation for w.
// 5. The exterior's equation on X F_ext, and the tail of H with h - 1 taken as h - 2, are not 0.
// 6. The stress of the paper's code: with M_X = U, I_X = H, J_X = U H, S_X = U^2 - X F^2 and Pi_X = F^2,
//    V = X v0 = L^-1 (2 eta X U - 2 D eta M - d M_eta) has V_X = C, and with
//        W = 1 - (2 D eta M + d M_eta) / X,
//        F Q_s = -F W + [(1 - h) I - D eta I_eta - d J_eta + 2 (h - D) eta J] / (2 X^2),
//        N_s = -W U + [D (M - eta M_eta) + 4 h eta S - d S_eta] / X + 4 A eta Pi - d Pi_eta,
//        X T0_theta = X (X F Q_s / L + 2 X F_X),   sqrt(2X) T0_z = X N_s / L + 2 X U_X,
//    (X T0_theta)_X = -X R_theta and (sqrt(2X) T0_z)_X = -R_z for any F and U: the stress is minus the integrated
//    residuals, the torque and the force of 1, and with (1 - h) I taken as (1 - 2 h) I it is not.
// 7. Every exact value is stored in the build's width.
// 8. Every identity is written whole to the cfg's record.
// The request: matching_rule <cfg>.
//     bash examples/navier_stokes/run.sh matching_rule examples/navier_stokes/cfg/matching_rule.cfg

// Each call below writes its expression into `out`, which may be one of its operands, and gives back every cell it
// takes for itself before it returns (jet_rule.h).

static void matching_rule_x(long long numerator, long long denominator, JetRule *out)
{
    jet_rule_power(JET_RULE_BASE_X, numerator, denominator, 0ll, 1ll, out);
}

static void matching_rule_end(JetRule *out)
{
    jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll, out);
}

static void matching_rule_in_h(long long whole, long long part, JetRule *out)
{
    JetRule h = JET_RULE_NONE;
    JetRule number = JET_RULE_NONE;
    jet_rule_h(&h);
    jet_rule_scaled(&h, sim_rational(part, 1ll), &h);
    jet_rule_number(whole, 1ll, &number);
    jet_rule_sum(&number, &h, out);
    jet_rule_released(&h);
    jet_rule_released(&number);
}

// `left` times (`middle` times `right`)
static void matching_rule_times3(const JetRule *left, const JetRule *middle, const JetRule *right, JetRule *out)
{
    JetRule inner = JET_RULE_NONE;
    jet_rule_product(middle, right, &inner);
    jet_rule_product(left, &inner, out);
    jet_rule_released(&inner);
}

// `rule` times `factor`, summed into `into`, or taken from it where `sign` is -1
static void matching_rule_added(JetRule *into, const JetRule *rule, SimRational factor, int sign)
{
    JetRule scaled = JET_RULE_NONE;
    jet_rule_scaled(rule, factor, &scaled);
    if (sign > 0)
    {
        jet_rule_sum(into, &scaled, into);
    }
    else
    {
        jet_rule_difference(into, &scaled, into);
    }
    jet_rule_released(&scaled);
}

static void matching_rule_t(const JetRule *field, const JetRule *minus_b, JetRule *out)
{
    JetRule d = JET_RULE_NONE;
    JetRule part = JET_RULE_NONE;
    JetRule other = JET_RULE_NONE;
    JetRule inner = JET_RULE_NONE;
    jet_rule_number(1ll, 2ll, &d);
    jet_rule_h(&part);
    jet_rule_difference(&d, &part, &d);
    jet_rule_product(minus_b, field, &inner);
    jet_rule_eta(&part);
    jet_rule_product(&d, &part, &part);
    jet_rule_slope_eta(field, &other);
    jet_rule_product(&part, &other, &part);
    jet_rule_sum(&inner, &part, &inner);
    matching_rule_x(1ll, 1ll, &part);
    jet_rule_x(field, &other);
    jet_rule_product(&part, &other, &part);
    jet_rule_sum(&inner, &part, &inner);
    jet_rule_over_l(&part);
    jet_rule_product(&part, &inner, out);
    JetRule *const done[4] = {&d, &part, &other, &inner};
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        jet_rule_released(done[each]);
    }
}

static void matching_rule_z(const JetRule *field, const JetRule *two_b, JetRule *out)
{
    JetRule part = JET_RULE_NONE;
    JetRule other = JET_RULE_NONE;
    JetRule inner = JET_RULE_NONE;
    jet_rule_eta(&part);
    jet_rule_product(two_b, &part, &part);
    jet_rule_product(&part, field, &inner);
    matching_rule_end(&part);
    jet_rule_slope_eta(field, &other);
    jet_rule_product(&part, &other, &part);
    jet_rule_sum(&inner, &part, &inner);
    jet_rule_eta(&part);
    matching_rule_x(1ll, 1ll, &other);
    jet_rule_product(&part, &other, &part);
    jet_rule_x(field, &other);
    jet_rule_product(&part, &other, &part);
    matching_rule_added(&inner, &part, sim_rational(2ll, 1ll), -1);
    jet_rule_over_l(&part);
    jet_rule_product(&part, &inner, out);
    JetRule *const done[3] = {&part, &other, &inner};
    for (unsigned int each = 0u; each < 3u; each += 1u)
    {
        jet_rule_released(done[each]);
    }
}

// 2^(-a - b h) X^(a + b h) d^(-a - b h), z^(a + b h) for z = X / (2 d)
static void matching_rule_z_power(long long a, long long b, JetRule *out)
{
    JetRule two = JET_RULE_NONE;
    JetRule x = JET_RULE_NONE;
    JetRule d = JET_RULE_NONE;
    jet_rule_power(JET_RULE_BASE_TWO, -a, 1ll, -b, 1ll, &two);
    jet_rule_power(JET_RULE_BASE_X, a, 1ll, b, 1ll, &x);
    jet_rule_power(JET_RULE_BASE_D, -a, 1ll, -b, 1ll, &d);
    matching_rule_times3(&two, &x, &d, out);
    jet_rule_released(&two);
    jet_rule_released(&x);
    jet_rule_released(&d);
}

static int matching_rule_report(SimResults *results, int passed, const char *pass, const char *fail, const char *what)
{
    mmgr_line_put(&results->line, passed ? pass : fail);
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
    sim_check(results, passed, what);
    return passed;
}

int program_main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    if ((count != 2) || !run_cfg_open(arguments[1], &cfg, &results.line))
    {
        fprintf(stderr, "matching_rule <cfg>\n");
        return 2;
    }
    // Every expression of the run, each given back as the run ends; A to D hold a term that an expression reads once
    enum
    {
        X,
        ETA,
        H,
        END,
        TWO_A,
        F,
        U,
        C_INNER,
        C,
        V,
        PI,
        INFLOW,
        THETA_TWO_B,
        T_F,
        Z_F,
        X_F,
        THETA,
        TORQUE_BRACKET,
        TORQUE_LOCAL,
        TORQUE,
        AXIAL_TWO_B,
        PRESSURE_TWO_B,
        T_U,
        Z_U,
        ALONG,
        FORCE_BRACKET,
        PRESSURE_PART,
        FORCE_LOCAL,
        X_F_F,
        PRESSURE_LOCAL,
        FORCE,
        CHANGED,
        AMPLITUDE,
        EXTERIOR,
        OUTSIDE,
        Z,
        Z_X,
        W,
        H_TAIL,
        C_C,
        S_TAIL,
        TWO_D,
        INNER,
        ROOT_TWO_C,
        POWER_TAIL,
        H_FIELD,
        MOVED,
        MOVED_OUTSIDE,
        MOVED_TAIL,
        H_X,
        M,
        I_HEAT,
        J,
        S,
        D_HALF,
        A_HALF,
        MOMENT_FLUX,
        VOLUME_M,
        INFLOW_M,
        THETA_M,
        ALONG_M,
        OVER_X,
        W_STRESS,
        BRACKET,
        F_Q,
        X_T_THETA,
        N_BRACKET,
        N_S,
        ROOT_T_Z,
        STRESS_THETA,
        STRESS_Z,
        CHANGED_STRESS,
        SCRATCH_A,
        SCRATCH_B,
        SCRATCH_C,
        SCRATCH_D,
        RULES
    };
    static JetRule s_rule[RULES];
    for (unsigned int each = 0u; each < RULES; each += 1u)
    {
        s_rule[each] = JetRule JET_RULE_NONE;
    }
    JetRule *const r = s_rule;
    JetRule *const a = &r[SCRATCH_A];
    JetRule *const b = &r[SCRATCH_B];
    JetRule *const c3 = &r[SCRATCH_C];
    JetRule *const d4 = &r[SCRATCH_D];
    const SimRational two = sim_rational(2ll, 1ll);
    matching_rule_x(1ll, 1ll, &r[X]);
    jet_rule_eta(&r[ETA]);
    jet_rule_h(&r[H]);
    matching_rule_end(&r[END]);
    matching_rule_in_h(1ll, 2ll, &r[TWO_A]);
    const unsigned int angular = jet_rule_function("F");
    const unsigned int axial = jet_rule_function("U");
    const unsigned int volume = jet_rule_function("V");
    const unsigned int pressure = jet_rule_function("Pi");
    jet_rule_slope(angular, 0u, 0u, &r[F]);
    jet_rule_slope(axial, 0u, 0u, &r[U]);
    // c = L^-1 ((2A eta) U - d U_eta + 2 (eta X) U_X)
    jet_rule_product(&r[TWO_A], &r[ETA], a);
    jet_rule_product(a, &r[U], &r[C_INNER]);
    jet_rule_slope_eta(&r[U], a);
    jet_rule_product(&r[END], a, a);
    jet_rule_difference(&r[C_INNER], a, &r[C_INNER]);
    jet_rule_product(&r[ETA], &r[X], a);
    jet_rule_x(&r[U], b);
    jet_rule_product(a, b, a);
    matching_rule_added(&r[C_INNER], a, two, 1);
    jet_rule_over_l(a);
    jet_rule_product(a, &r[C_INNER], &r[C]);
    jet_rule_relate(volume, 1u, &r[C]);
    jet_rule_product(&r[F], &r[F], a);
    jet_rule_relate(pressure, 1u, a);
    jet_rule_slope(volume, 0u, 0u, &r[V]);
    jet_rule_slope(pressure, 0u, 0u, &r[PI]);
    matching_rule_x(-1ll, 1ll, a);
    jet_rule_product(&r[V], a, &r[INFLOW]);

    matching_rule_in_h(-2ll, -2ll, &r[THETA_TWO_B]);
    matching_rule_in_h(1ll, 1ll, a);
    matching_rule_t(&r[F], a, &r[T_F]);
    matching_rule_z(&r[F], &r[THETA_TWO_B], &r[Z_F]);
    jet_rule_product(&r[X], &r[F], &r[X_F]);
    // theta = T F + inflow (X F_X + F) + U Z F - 2 (X F)_XX
    jet_rule_x(&r[F], a);
    jet_rule_product(&r[X], a, a);
    jet_rule_sum(a, &r[F], a);
    jet_rule_product(&r[INFLOW], a, a);
    jet_rule_sum(&r[T_F], a, &r[THETA]);
    jet_rule_product(&r[U], &r[Z_F], a);
    jet_rule_sum(&r[THETA], a, &r[THETA]);
    jet_rule_x(&r[X_F], a);
    jet_rule_x(a, a);
    matching_rule_added(&r[THETA], a, two, -1);
    // the torque's bracket V X F - 2 X^2 F_X, and its integrand
    jet_rule_product(&r[V], &r[X_F], &r[TORQUE_BRACKET]);
    matching_rule_x(2ll, 1ll, a);
    jet_rule_x(&r[F], b);
    jet_rule_product(a, b, a);
    matching_rule_added(&r[TORQUE_BRACKET], a, two, -1);
    jet_rule_product(&r[X], &r[T_F], a);
    jet_rule_product(&r[U], &r[Z_F], b);
    jet_rule_product(&r[X], b, b);
    jet_rule_sum(a, b, &r[TORQUE_LOCAL]);
    jet_rule_product(&r[C], &r[X_F], a);
    jet_rule_difference(&r[TORQUE_LOCAL], a, &r[TORQUE_LOCAL]);
    jet_rule_product(&r[X], &r[THETA], a);
    jet_rule_x(&r[TORQUE_BRACKET], b);
    jet_rule_difference(a, b, a);
    jet_rule_difference(a, &r[TORQUE_LOCAL], &r[TORQUE]);

    matching_rule_in_h(-1ll, -2ll, &r[AXIAL_TWO_B]);
    matching_rule_in_h(-2ll, -4ll, &r[PRESSURE_TWO_B]);
    jet_rule_scaled(&r[TWO_A], sim_rational(1ll, 2ll), a);
    matching_rule_t(&r[U], a, &r[T_U]);
    matching_rule_z(&r[U], &r[AXIAL_TWO_B], &r[Z_U]);
    jet_rule_x(&r[U], a);
    jet_rule_product(&r[V], a, a);
    jet_rule_sum(&r[T_U], a, &r[ALONG]);
    jet_rule_product(&r[U], &r[Z_U], a);
    jet_rule_sum(&r[ALONG], a, &r[ALONG]);
    matching_rule_z(&r[PI], &r[PRESSURE_TWO_B], a);
    jet_rule_sum(&r[ALONG], a, &r[ALONG]);
    jet_rule_x(&r[U], a);
    jet_rule_product(&r[X], a, a);
    jet_rule_x(a, a);
    matching_rule_added(&r[ALONG], a, two, -1);
    // the force's bracket and its integrand
    jet_rule_product(&r[V], &r[U], &r[FORCE_BRACKET]);
    jet_rule_x(&r[U], a);
    jet_rule_product(&r[X], a, a);
    matching_rule_added(&r[FORCE_BRACKET], a, two, -1);
    jet_rule_product(&r[PRESSURE_TWO_B], &r[ETA], a);
    jet_rule_product(&r[X], &r[PI], b);
    jet_rule_product(a, b, &r[PRESSURE_PART]);
    jet_rule_slope_eta(&r[PI], a);
    jet_rule_product(&r[X], a, a);
    jet_rule_product(&r[END], a, a);
    jet_rule_sum(&r[PRESSURE_PART], a, &r[PRESSURE_PART]);
    jet_rule_over_l(a);
    jet_rule_product(a, &r[PRESSURE_PART], a);
    jet_rule_sum(&r[FORCE_BRACKET], a, &r[FORCE_BRACKET]);
    jet_rule_product(&r[U], &r[Z_U], a);
    jet_rule_sum(&r[T_U], a, &r[FORCE_LOCAL]);
    jet_rule_product(&r[C], &r[U], a);
    jet_rule_difference(&r[FORCE_LOCAL], a, &r[FORCE_LOCAL]);
    jet_rule_product(&r[F], &r[F], a);
    jet_rule_product(&r[X], a, &r[X_F_F]);
    jet_rule_product(&r[PRESSURE_TWO_B], &r[ETA], a);
    jet_rule_product(a, &r[X_F_F], &r[PRESSURE_LOCAL]);
    jet_rule_slope_eta(&r[F], a);
    jet_rule_product(&r[F], a, a);
    jet_rule_product(&r[X], a, a);
    jet_rule_product(&r[END], a, a);
    matching_rule_added(&r[PRESSURE_LOCAL], a, two, 1);
    jet_rule_product(&r[ETA], &r[X_F_F], a);
    matching_rule_added(&r[PRESSURE_LOCAL], a, two, 1);
    jet_rule_over_l(a);
    jet_rule_product(a, &r[PRESSURE_LOCAL], a);
    jet_rule_difference(&r[FORCE_LOCAL], a, &r[FORCE_LOCAL]);
    jet_rule_x(&r[FORCE_BRACKET], a);
    jet_rule_difference(&r[ALONG], a, a);
    jet_rule_difference(a, &r[FORCE_LOCAL], &r[FORCE]);
    const int torque_zero = jet_rule_zero(&r[TORQUE]);
    const int force_zero = jet_rule_zero(&r[FORCE]);
    mmgr_line_put(&results.line, "  the parts of the torque: ");
    mmgr_line_put(&results.line, torque_zero ? "g - G_X - q is 0" : "g - G_X - q is not 0");
    mmgr_line_put(&results.line, "; of the force: ");
    mmgr_line_put(&results.line, force_zero ? "0" : "not 0");
    mmgr_line_put(&results.line, ", for any F and U\n");
    sim_flush(&results);
    sim_check(&results, torque_zero && force_zero, "the parts of the torque and the force");

    jet_rule_product(&r[C], &r[F], a);
    jet_rule_sum(&r[TORQUE], a, &r[CHANGED]);
    matching_rule_report(&results, !jet_rule_zero(&r[CHANGED]), "  the torque with one part changed is not 0",
                         "  the torque with one part changed is still 0", "a changed part seen");

    jet_rule_constant("c", &r[AMPLITUDE]);
    jet_rule_power(JET_RULE_BASE_TWO, -3ll, 2ll, -1ll, 1ll, a);
    jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, -1ll, 1ll, b);
    jet_rule_w(0u, c3);
    jet_rule_product(b, c3, b);
    jet_rule_product(a, b, a);
    jet_rule_product(&r[AMPLITUDE], a, &r[EXTERIOR]);
    matching_rule_in_h(1ll, 1ll, a);
    matching_rule_t(&r[EXTERIOR], a, a);
    jet_rule_product(&r[X], &r[EXTERIOR], b);
    jet_rule_x(b, b);
    jet_rule_x(b, b);
    jet_rule_scaled(b, two, b);
    jet_rule_difference(a, b, &r[OUTSIDE]);
    const int outside_zero = jet_rule_zero(&r[OUTSIDE]);
    matching_rule_report(&results, outside_zero, "  F_ext with Kummer's w solves T_{-(A+1/2)} F - 2 (X F)_XX = 0",
                         "  F_ext does not solve T_{-(A+1/2)} F - 2 (X F)_XX = 0", "the exterior's equation");

    jet_rule_power(JET_RULE_BASE_TWO, -1ll, 1ll, 0ll, 1ll, a);
    jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, 0ll, 1ll, b);
    matching_rule_times3(a, &r[X], b, &r[Z]);
    jet_rule_product(a, b, &r[Z_X]);
    jet_rule_w(0u, &r[W]);
    // h_tail = z^2 (w' - w) + z^(1-h), less its integrand
    jet_rule_w(1u, a);
    jet_rule_difference(a, &r[W], a);
    matching_rule_z_power(2ll, 0ll, b);
    jet_rule_product(b, a, a);
    matching_rule_z_power(1ll, -1ll, b);
    jet_rule_sum(a, b, &r[H_TAIL]);
    jet_rule_x(&r[H_TAIL], a);
    jet_rule_product(&r[Z], &r[W], b);
    matching_rule_z_power(0ll, -1ll, c3);
    jet_rule_difference(b, c3, b);
    jet_rule_product(b, &r[Z_X], b);
    matching_rule_in_h(-1ll, 1ll, c3);
    jet_rule_product(c3, b, b);
    jet_rule_difference(a, b, &r[H_TAIL]);
    jet_rule_product(&r[AMPLITUDE], &r[AMPLITUDE], &r[C_C]);
    // s_tail
    jet_rule_product(&r[EXTERIOR], &r[EXTERIOR], a);
    matching_rule_times3(&r[H], &r[X], a, a);
    jet_rule_scaled(a, sim_rational(4ll, 1ll), &r[S_TAIL]);
    jet_rule_power(JET_RULE_BASE_X, 0ll, 1ll, -2ll, 1ll, a);
    jet_rule_product(&r[C_C], a, a);
    jet_rule_x(a, a);
    jet_rule_sum(&r[S_TAIL], a, &r[S_TAIL]);
    jet_rule_power(JET_RULE_BASE_TWO, 0ll, 1ll, -2ll, 1ll, a);
    jet_rule_power(JET_RULE_BASE_D, 0ll, 1ll, -2ll, 1ll, b);
    jet_rule_product(a, b, &r[TWO_D]);
    jet_rule_product(&r[W], &r[W], a);
    jet_rule_product(&r[Z], a, a);
    matching_rule_z_power(-1ll, -2ll, b);
    jet_rule_difference(a, b, &r[INNER]);
    jet_rule_product(&r[INNER], &r[Z_X], a);
    jet_rule_product(&r[TWO_D], a, a);
    jet_rule_product(&r[C_C], a, a);
    jet_rule_product(&r[H], a, a);
    matching_rule_added(&r[S_TAIL], a, two, -1);
    jet_rule_power(JET_RULE_BASE_TWO, 1ll, 2ll, 0ll, 1ll, a);
    jet_rule_product(a, &r[AMPLITUDE], &r[ROOT_TWO_C]);
    // power_tail = (1 - h) sqrt2 c X^(-h) - (sqrt2 c X^(1-h))_X
    jet_rule_power(JET_RULE_BASE_X, 0ll, 1ll, -1ll, 1ll, a);
    jet_rule_product(&r[ROOT_TWO_C], a, a);
    matching_rule_in_h(1ll, -1ll, b);
    jet_rule_product(b, a, a);
    jet_rule_power(JET_RULE_BASE_X, 1ll, 1ll, -1ll, 1ll, b);
    jet_rule_product(&r[ROOT_TWO_C], b, b);
    jet_rule_x(b, b);
    jet_rule_difference(a, b, &r[POWER_TAIL]);
    // h_field = 2 X F_ext - sqrt2 c 2^(-1-h) d^(-1-h) X w
    jet_rule_product(&r[X], &r[EXTERIOR], a);
    jet_rule_scaled(a, two, a);
    jet_rule_power(JET_RULE_BASE_TWO, -1ll, 1ll, -1ll, 1ll, b);
    jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, -1ll, 1ll, c3);
    jet_rule_product(&r[X], &r[W], d4);
    jet_rule_product(c3, d4, c3);
    jet_rule_product(b, c3, b);
    jet_rule_product(&r[ROOT_TWO_C], b, b);
    jet_rule_difference(a, b, &r[H_FIELD]);
    const int tails = jet_rule_zero(&r[H_TAIL]) && jet_rule_zero(&r[S_TAIL]) && jet_rule_zero(&r[POWER_TAIL]) &&
                      jet_rule_zero(&r[H_FIELD]);
    matching_rule_report(&results, tails,
                         "  the tails of H and S and the integral of H_pow are valid with Kummer's equation",
                         "  a tail identity is not valid", "the tails");
    jet_rule_product(&r[X], &r[EXTERIOR], &r[MOVED]);
    matching_rule_in_h(1ll, 1ll, a);
    matching_rule_t(&r[MOVED], a, a);
    jet_rule_product(&r[X], &r[MOVED], b);
    jet_rule_x(b, b);
    jet_rule_x(b, b);
    jet_rule_scaled(b, two, b);
    jet_rule_difference(a, b, &r[MOVED_OUTSIDE]);
    jet_rule_product(&r[Z], &r[W], a);
    matching_rule_z_power(0ll, -1ll, b);
    jet_rule_difference(a, b, a);
    jet_rule_product(a, &r[Z_X], a);
    jet_rule_difference(&r[H_TAIL], a, &r[MOVED_TAIL]);
    matching_rule_report(&results, !jet_rule_zero(&r[MOVED_OUTSIDE]) && !jet_rule_zero(&r[MOVED_TAIL]),
                         "  the exterior's equation on X F_ext and the tail of H with h - 2 are not 0",
                         "  a changed exterior or tail is still 0", "a changed exterior and tail seen");

    const unsigned int moment = jet_rule_function("M");
    const unsigned int heat = jet_rule_function("I");
    const unsigned int carried = jet_rule_function("J");
    const unsigned int square = jet_rule_function("S");
    jet_rule_scaled(&r[X_F], two, &r[H_X]);
    jet_rule_relate(moment, 1u, &r[U]);
    jet_rule_relate(heat, 1u, &r[H_X]);
    jet_rule_product(&r[U], &r[H_X], a);
    jet_rule_relate(carried, 1u, a);
    jet_rule_product(&r[U], &r[U], a);
    jet_rule_product(&r[F], &r[F], b);
    jet_rule_product(&r[X], b, b);
    jet_rule_difference(a, b, a);
    jet_rule_relate(square, 1u, a);
    jet_rule_slope(moment, 0u, 0u, &r[M]);
    jet_rule_slope(heat, 0u, 0u, &r[I_HEAT]);
    jet_rule_slope(carried, 0u, 0u, &r[J]);
    jet_rule_slope(square, 0u, 0u, &r[S]);
    jet_rule_number(1ll, 2ll, a);
    jet_rule_difference(a, &r[H], &r[D_HALF]);
    jet_rule_sum(a, &r[H], &r[A_HALF]);
    // moment_flux = 2 D eta M + d M_eta
    jet_rule_product(&r[ETA], &r[M], a);
    jet_rule_product(&r[D_HALF], a, a);
    jet_rule_scaled(a, two, a);
    jet_rule_slope_eta(&r[M], b);
    jet_rule_product(&r[END], b, b);
    jet_rule_sum(a, b, &r[MOMENT_FLUX]);
    jet_rule_product(&r[X], &r[U], a);
    jet_rule_product(&r[ETA], a, a);
    jet_rule_scaled(a, two, a);
    jet_rule_difference(a, &r[MOMENT_FLUX], a);
    jet_rule_over_l(b);
    jet_rule_product(b, a, &r[VOLUME_M]);
    jet_rule_x(&r[VOLUME_M], a);
    jet_rule_difference(a, &r[C], a);
    const int continuity = jet_rule_zero(a);
    matching_rule_x(-1ll, 1ll, a);
    jet_rule_product(&r[VOLUME_M], a, &r[INFLOW_M]);
    jet_rule_x(&r[F], a);
    jet_rule_product(&r[X], a, a);
    jet_rule_sum(a, &r[F], a);
    jet_rule_product(&r[INFLOW_M], a, a);
    jet_rule_sum(&r[T_F], a, &r[THETA_M]);
    jet_rule_product(&r[U], &r[Z_F], a);
    jet_rule_sum(&r[THETA_M], a, &r[THETA_M]);
    jet_rule_x(&r[X_F], a);
    jet_rule_x(a, a);
    matching_rule_added(&r[THETA_M], a, two, -1);
    jet_rule_x(&r[U], a);
    jet_rule_product(&r[VOLUME_M], a, a);
    jet_rule_sum(&r[T_U], a, &r[ALONG_M]);
    jet_rule_product(&r[U], &r[Z_U], a);
    jet_rule_sum(&r[ALONG_M], a, &r[ALONG_M]);
    matching_rule_z(&r[PI], &r[PRESSURE_TWO_B], a);
    jet_rule_sum(&r[ALONG_M], a, &r[ALONG_M]);
    jet_rule_x(&r[U], a);
    jet_rule_product(&r[X], a, a);
    jet_rule_x(a, a);
    matching_rule_added(&r[ALONG_M], a, two, -1);
    matching_rule_x(-1ll, 1ll, &r[OVER_X]);
    jet_rule_number(1ll, 1ll, a);
    jet_rule_product(&r[MOMENT_FLUX], &r[OVER_X], b);
    jet_rule_difference(a, b, &r[W_STRESS]);
    // bracket = (1 - h) I - D eta I_eta - d J_eta + 2 (h - D) eta J
    jet_rule_number(1ll, 1ll, a);
    jet_rule_difference(a, &r[H], a);
    jet_rule_product(a, &r[I_HEAT], &r[BRACKET]);
    jet_rule_slope_eta(&r[I_HEAT], a);
    jet_rule_product(&r[ETA], a, a);
    jet_rule_product(&r[D_HALF], a, a);
    jet_rule_difference(&r[BRACKET], a, &r[BRACKET]);
    jet_rule_slope_eta(&r[J], a);
    jet_rule_product(&r[END], a, a);
    jet_rule_difference(&r[BRACKET], a, &r[BRACKET]);
    jet_rule_difference(&r[H], &r[D_HALF], a);
    jet_rule_scaled(a, two, a);
    jet_rule_product(&r[ETA], &r[J], b);
    jet_rule_product(a, b, a);
    jet_rule_sum(&r[BRACKET], a, &r[BRACKET]);
    // F Q_s = -F W + bracket / (2 X^2)
    jet_rule_product(&r[F], &r[W_STRESS], a);
    jet_rule_scaled(a, sim_rational(-1ll, 1ll), a);
    matching_rule_x(-2ll, 1ll, b);
    jet_rule_product(&r[BRACKET], b, b);
    jet_rule_scaled(b, sim_rational(1ll, 2ll), b);
    jet_rule_sum(a, b, &r[F_Q]);
    jet_rule_product(&r[X], &r[F_Q], a);
    jet_rule_over_l(b);
    jet_rule_product(b, a, a);
    jet_rule_x(&r[F], b);
    jet_rule_product(&r[X], b, b);
    jet_rule_scaled(b, two, b);
    jet_rule_sum(a, b, a);
    jet_rule_product(&r[X], a, &r[X_T_THETA]);
    // n_bracket = D (M - eta M_eta) + 4 h eta S - d S_eta
    jet_rule_slope_eta(&r[M], a);
    jet_rule_product(&r[ETA], a, a);
    jet_rule_difference(&r[M], a, a);
    jet_rule_product(&r[D_HALF], a, &r[N_BRACKET]);
    jet_rule_product(&r[ETA], &r[S], a);
    jet_rule_product(&r[H], a, a);
    matching_rule_added(&r[N_BRACKET], a, sim_rational(4ll, 1ll), 1);
    jet_rule_slope_eta(&r[S], a);
    jet_rule_product(&r[END], a, a);
    jet_rule_difference(&r[N_BRACKET], a, &r[N_BRACKET]);
    // N_s = -W U + n_bracket / X + 4 A eta Pi - d Pi_eta
    jet_rule_product(&r[W_STRESS], &r[U], a);
    jet_rule_scaled(a, sim_rational(-1ll, 1ll), &r[N_S]);
    jet_rule_product(&r[N_BRACKET], &r[OVER_X], a);
    jet_rule_sum(&r[N_S], a, &r[N_S]);
    jet_rule_product(&r[ETA], &r[PI], a);
    jet_rule_product(&r[A_HALF], a, a);
    matching_rule_added(&r[N_S], a, sim_rational(4ll, 1ll), 1);
    jet_rule_slope_eta(&r[PI], a);
    jet_rule_product(&r[END], a, a);
    jet_rule_difference(&r[N_S], a, &r[N_S]);
    jet_rule_product(&r[X], &r[N_S], a);
    jet_rule_over_l(b);
    jet_rule_product(b, a, a);
    jet_rule_x(&r[U], b);
    jet_rule_product(&r[X], b, b);
    jet_rule_scaled(b, two, b);
    jet_rule_sum(a, b, &r[ROOT_T_Z]);
    jet_rule_x(&r[X_T_THETA], a);
    jet_rule_product(&r[X], &r[THETA_M], b);
    jet_rule_sum(a, b, &r[STRESS_THETA]);
    jet_rule_x(&r[ROOT_T_Z], a);
    jet_rule_sum(a, &r[ALONG_M], &r[STRESS_Z]);
    const int stress = continuity && jet_rule_zero(&r[STRESS_THETA]) && jet_rule_zero(&r[STRESS_Z]);
    mmgr_line_put(&results.line, "  continuity from M: ");
    mmgr_line_put(&results.line, continuity ? "V_X = C" : "V_X differs from C");
    mmgr_line_put(&results.line, "; the stress of the five integrals against the residuals: theta ");
    mmgr_line_put(&results.line, jet_rule_zero(&r[STRESS_THETA]) ? "equal" : "differs");
    mmgr_line_put(&results.line, ", z ");
    mmgr_line_put(&results.line, jet_rule_zero(&r[STRESS_Z]) ? "equal" : "differs");
    mmgr_line_put(&results.line, ", for any F and U\n");
    sim_flush(&results);
    sim_check(&results, stress, "the stress of the five integrals against the residuals");
    jet_rule_product(&r[H], &r[I_HEAT], a);
    jet_rule_over_l(b);
    jet_rule_product(b, a, a);
    jet_rule_scaled(a, sim_rational(-1ll, 2ll), a);
    jet_rule_x(a, a);
    jet_rule_sum(&r[STRESS_THETA], a, &r[CHANGED_STRESS]);
    matching_rule_report(&results, !jet_rule_zero(&r[CHANGED_STRESS]),
                         "  the stress with (1 - h) I taken as (1 - 2 h) I is not 0",
                         "  the stress with a changed part is still 0", "a changed stress seen");

    const int fits = !jet_rule_short() && !run_cfg_short() && !record_short();
    matching_rule_report(&results, fits, "  every exact value is stored in the build's width",
                         "  a value outgrew the build's width or an exponent passed a word",
                         "every exact value stored");
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        static const char *const s_written_names[12] = {"X R_theta",
                                                        "the torque's bracket G",
                                                        "the torque's integrand q",
                                                        "the torque's g - G_X - q",
                                                        "R_z",
                                                        "the force's bracket G",
                                                        "the force's integrand q",
                                                        "the force's g - G_X - q",
                                                        "F_ext",
                                                        "the exterior's equation on F_ext",
                                                        "the tail of H, its identity's derivative",
                                                        "the tail of S, its identity's derivative"};
        const JetRule *const written[12] = {&r[THETA],    &r[TORQUE_BRACKET], &r[TORQUE_LOCAL], &r[TORQUE],
                                            &r[ALONG],    &r[FORCE_BRACKET],  &r[FORCE_LOCAL],  &r[FORCE],
                                            &r[EXTERIOR], &r[OUTSIDE],        &r[H_TAIL],       &r[S_TAIL]};
        for (unsigned int each = 0u; each < 12u; each += 1u)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "expression ");
            mmgr_line_put(&line.line, s_written_names[each]);
            mmgr_line_put(&line.line, ", ");
            mmgr_line_decimal(&line.line, jet_rule_terms(written[each]), 1u);
            mmgr_line_put(&line.line, " terms");
            record_line_ended(record, &line);
            record_line_begun(&line);
            jet_rule_text(written[each], &line.line);
            record_line_ended(record, &line);
        }
        recorded = record_close(record);
    }
    matching_rule_report(&results, recorded, "  every identity is written whole to the cfg's record",
                         "  the record is not written: the cfg names none, or it does not open", "record written");
    for (unsigned int each = 0u; each < RULES; each += 1u)
    {
        jet_rule_released(&s_rule[each]);
    }
    return sim_close(&results, "matching rule");
}
