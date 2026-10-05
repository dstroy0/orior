// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching_rule.cu: the exterior, its tails and the parts of the six matching functions of Duraiswami's matched core
// as identities among held functions (jet_rule.h)
#include "run_cfg.h"

#include "jet_rule.h"
#include "record.h"

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
// 1. The torque's and the force's parts: g - G_X - q is 0 for any F and U, V and Pi held by their relations.
// 2. The same with one part of the torque's changed is not 0.
// 3. F_ext with Kummer's w solves the exterior's equation T_{-(A+1/2)} F - 2 (X F)_XX = 0.
// 4. The tails: each identity's derivative in X is 0, with Kummer's equation for w.
// 5. The exterior's equation on X F_ext, and the tail of H with h - 1 taken as h - 2, are not 0.
// 6. Every exact value is held in the build's width.
// 7. Every identity is written whole to the cfg's record.
// The request: matching_rule <cfg>.
//     bash examples/navier_stokes/run.sh matching_rule examples/navier_stokes/cfg/matching_rule.cfg

static JetRule matching_rule_number(long long numerator, long long denominator)
{
    return jet_rule_number(numerator, denominator);
}

static JetRule matching_rule_times(const JetRule &left, const JetRule &right)
{
    return jet_rule_product(left, right);
}

static JetRule matching_rule_x(long long numerator, long long denominator)
{
    return jet_rule_power(JET_RULE_BASE_X, numerator, denominator, 0ll, 1ll);
}

// d = 1 - eta^2, held as the base d
static JetRule matching_rule_end(void)
{
    return jet_rule_power(JET_RULE_BASE_D, 1ll, 1ll, 0ll, 1ll);
}

// a + b h
static JetRule matching_rule_in_h(long long whole, long long part)
{
    return jet_rule_sum(matching_rule_number(whole, 1ll), jet_rule_scaled(jet_rule_h(), sim_rational(part, 1ll)));
}

// T_b f = L^-1 (-b f + D eta f_eta + X f_X), -b given, D = 1/2 - h
static JetRule matching_rule_t(const JetRule &field, const JetRule &minus_b)
{
    const JetRule d = jet_rule_difference(matching_rule_number(1ll, 2ll), jet_rule_h());
    JetRule inner = matching_rule_times(minus_b, field);
    inner = jet_rule_sum(inner, matching_rule_times(matching_rule_times(d, jet_rule_eta()), jet_rule_slope_eta(field)));
    inner = jet_rule_sum(inner, matching_rule_times(matching_rule_x(1ll, 1ll), jet_rule_x(field)));
    return matching_rule_times(jet_rule_over_l(), inner);
}

// Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X), 2 b given
static JetRule matching_rule_z(const JetRule &field, const JetRule &two_b)
{
    JetRule inner = matching_rule_times(matching_rule_times(two_b, jet_rule_eta()), field);
    inner = jet_rule_sum(inner, matching_rule_times(matching_rule_end(), jet_rule_slope_eta(field)));
    inner = jet_rule_difference(inner, jet_rule_scaled(matching_rule_times(matching_rule_times(jet_rule_eta(), matching_rule_x(1ll, 1ll)), jet_rule_x(field)),
                                                       sim_rational(2ll, 1ll)));
    return matching_rule_times(jet_rule_over_l(), inner);
}

static int matching_rule_report(SimResults *results, int passed, const char *pass, const char *fail, const char *what)
{
    scriptura_text(&results->line, passed ? pass : fail);
    scriptura_character(&results->line, '\n');
    sim_flush(results);
    sim_check(results, passed, what);
    return passed;
}

int main(int count, char **arguments)
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
    const JetRule x = matching_rule_x(1ll, 1ll);
    const JetRule eta = jet_rule_eta();
    const JetRule h = jet_rule_h();
    const JetRule end = matching_rule_end();
    const JetRule two_a = matching_rule_in_h(1ll, 2ll);
    const unsigned int angular = jet_rule_function("F");
    const unsigned int axial = jet_rule_function("U");
    const unsigned int volume = jet_rule_function("V");
    const unsigned int pressure = jet_rule_function("Pi");
    const JetRule f = jet_rule_slope(angular, 0u, 0u);
    const JetRule u = jet_rule_slope(axial, 0u, 0u);
    // C = L^-1 (2 A eta U - d U_eta + 2 eta X U_X), V_X = C, Pi_X = F^2
    JetRule c_inner = matching_rule_times(matching_rule_times(two_a, eta), u);
    c_inner = jet_rule_difference(c_inner, matching_rule_times(end, jet_rule_slope_eta(u)));
    c_inner = jet_rule_sum(c_inner, jet_rule_scaled(matching_rule_times(matching_rule_times(eta, x), jet_rule_x(u)), sim_rational(2ll, 1ll)));
    const JetRule c = matching_rule_times(jet_rule_over_l(), c_inner);
    jet_rule_relate(volume, 1u, c);
    jet_rule_relate(pressure, 1u, matching_rule_times(f, f));
    const JetRule v = jet_rule_slope(volume, 0u, 0u);
    const JetRule pi = jet_rule_slope(pressure, 0u, 0u);
    const JetRule inflow = matching_rule_times(v, matching_rule_x(-1ll, 1ll));

    // 1. the torque
    const JetRule theta_two_b = matching_rule_in_h(-2ll, -2ll);
    const JetRule t_f = matching_rule_t(f, matching_rule_in_h(1ll, 1ll));
    const JetRule z_f = matching_rule_z(f, theta_two_b);
    const JetRule x_f = matching_rule_times(x, f);
    JetRule theta = jet_rule_sum(t_f, matching_rule_times(inflow, jet_rule_sum(matching_rule_times(x, jet_rule_x(f)), f)));
    theta = jet_rule_sum(theta, matching_rule_times(u, z_f));
    theta = jet_rule_difference(theta, jet_rule_scaled(jet_rule_x(jet_rule_x(x_f)), sim_rational(2ll, 1ll)));
    const JetRule torque_bracket = jet_rule_difference(matching_rule_times(v, x_f),
                                                       jet_rule_scaled(matching_rule_times(matching_rule_x(2ll, 1ll), jet_rule_x(f)), sim_rational(2ll, 1ll)));
    JetRule torque_local = jet_rule_sum(matching_rule_times(x, t_f), matching_rule_times(x, matching_rule_times(u, z_f)));
    torque_local = jet_rule_difference(torque_local, matching_rule_times(c, x_f));
    const JetRule torque = jet_rule_difference(jet_rule_difference(matching_rule_times(x, theta), jet_rule_x(torque_bracket)), torque_local);

    // 1. the force, 2 b = -2 - 4 h in Z_{-2A}
    const JetRule axial_two_b = matching_rule_in_h(-1ll, -2ll);
    const JetRule pressure_two_b = matching_rule_in_h(-2ll, -4ll);
    const JetRule t_u = matching_rule_t(u, jet_rule_scaled(two_a, sim_rational(1ll, 2ll)));
    const JetRule z_u = matching_rule_z(u, axial_two_b);
    JetRule along = jet_rule_sum(t_u, matching_rule_times(v, jet_rule_x(u)));
    along = jet_rule_sum(along, matching_rule_times(u, z_u));
    along = jet_rule_sum(along, matching_rule_z(pi, pressure_two_b));
    along = jet_rule_difference(along, jet_rule_scaled(jet_rule_x(matching_rule_times(x, jet_rule_x(u))), sim_rational(2ll, 1ll)));
    JetRule force_bracket = jet_rule_difference(matching_rule_times(v, u), jet_rule_scaled(matching_rule_times(x, jet_rule_x(u)), sim_rational(2ll, 1ll)));
    JetRule pressure_part = matching_rule_times(matching_rule_times(pressure_two_b, eta), matching_rule_times(x, pi));
    pressure_part = jet_rule_sum(pressure_part, matching_rule_times(end, matching_rule_times(x, jet_rule_slope_eta(pi))));
    force_bracket = jet_rule_sum(force_bracket, matching_rule_times(jet_rule_over_l(), pressure_part));
    JetRule force_local = jet_rule_sum(t_u, matching_rule_times(u, z_u));
    force_local = jet_rule_difference(force_local, matching_rule_times(c, u));
    const JetRule x_f_f = matching_rule_times(x, matching_rule_times(f, f));
    JetRule pressure_local = matching_rule_times(matching_rule_times(pressure_two_b, eta), x_f_f);
    pressure_local = jet_rule_sum(pressure_local, jet_rule_scaled(matching_rule_times(end, matching_rule_times(x, matching_rule_times(f, jet_rule_slope_eta(f)))), sim_rational(2ll, 1ll)));
    pressure_local = jet_rule_sum(pressure_local, jet_rule_scaled(matching_rule_times(eta, x_f_f), sim_rational(2ll, 1ll)));
    force_local = jet_rule_difference(force_local, matching_rule_times(jet_rule_over_l(), pressure_local));
    const JetRule force = jet_rule_difference(jet_rule_difference(along, jet_rule_x(force_bracket)), force_local);
    const int torque_zero = jet_rule_zero(torque);
    const int force_zero = jet_rule_zero(force);
    scriptura_text(&results.line, "  the parts of the torque: ");
    scriptura_text(&results.line, torque_zero ? "g - G_X - q is 0" : "g - G_X - q is not 0");
    scriptura_text(&results.line, "; of the force: ");
    scriptura_text(&results.line, force_zero ? "0" : "not 0");
    scriptura_text(&results.line, ", for any F and U\n");
    sim_flush(&results);
    sim_check(&results, torque_zero && force_zero, "the parts of the torque and the force");

    // 2. a changed part
    const JetRule changed = jet_rule_sum(torque, matching_rule_times(c, f));
    matching_rule_report(&results, !jet_rule_zero(changed), "  the torque with one part changed is not 0",
                         "  the torque with one part changed is still 0", "a changed part seen");

    // 3. the exterior, F_ext = c 2^(-3/2 - h) d^(-1-h) w
    const JetRule amplitude = jet_rule_constant("c");
    const JetRule exterior = matching_rule_times(amplitude, matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, -3ll, 2ll, -1ll, 1ll),
                                                                                matching_rule_times(jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, -1ll, 1ll), jet_rule_w(0u))));
    const JetRule outside = jet_rule_difference(matching_rule_t(exterior, matching_rule_in_h(1ll, 1ll)),
                                                jet_rule_scaled(jet_rule_x(jet_rule_x(matching_rule_times(x, exterior))), sim_rational(2ll, 1ll)));
    const int outside_zero = jet_rule_zero(outside);
    matching_rule_report(&results, outside_zero, "  F_ext with Kummer's w solves T_{-(A+1/2)} F - 2 (X F)_XX = 0",
                         "  F_ext does not solve T_{-(A+1/2)} F - 2 (X F)_XX = 0", "the exterior's equation");

    // 4. the tails, z = X / (2 d), z_X = 1 / (2 d)
    const JetRule z = matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, -1ll, 1ll, 0ll, 1ll), matching_rule_times(x, jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, 0ll, 1ll)));
    const JetRule z_x = matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, -1ll, 1ll, 0ll, 1ll), jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, 0ll, 1ll));
    // z^p = 2^-p X^p d^-p for p = a + b h
    const auto z_power = [](long long a, long long b) {
        return jet_rule_product(jet_rule_power(JET_RULE_BASE_TWO, -a, 1ll, -b, 1ll),
                                jet_rule_product(jet_rule_power(JET_RULE_BASE_X, a, 1ll, b, 1ll), jet_rule_power(JET_RULE_BASE_D, -a, 1ll, -b, 1ll)));
    };
    const JetRule w = jet_rule_w(0u);
    JetRule h_tail = jet_rule_sum(matching_rule_times(z_power(2ll, 0ll), jet_rule_difference(jet_rule_w(1u), w)), z_power(1ll, -1ll));
    h_tail = jet_rule_difference(jet_rule_x(h_tail),
                                 matching_rule_times(matching_rule_in_h(-1ll, 1ll), matching_rule_times(jet_rule_difference(matching_rule_times(z, w), z_power(0ll, -1ll)), z_x)));
    // 4 h X F_ext^2 + d/dX (c^2 X^(-2h)) - 2 h c^2 (2d)^(-2h) (z w^2 - z^(-1-2h)) z_X
    const JetRule c_c = matching_rule_times(amplitude, amplitude);
    JetRule s_tail = jet_rule_scaled(matching_rule_times(h, matching_rule_times(x, matching_rule_times(exterior, exterior))), sim_rational(4ll, 1ll));
    s_tail = jet_rule_sum(s_tail, jet_rule_x(matching_rule_times(c_c, jet_rule_power(JET_RULE_BASE_X, 0ll, 1ll, -2ll, 1ll))));
    const JetRule two_d = matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, 0ll, 1ll, -2ll, 1ll), jet_rule_power(JET_RULE_BASE_D, 0ll, 1ll, -2ll, 1ll));
    const JetRule inner = jet_rule_difference(matching_rule_times(z, matching_rule_times(w, w)), z_power(-1ll, -2ll));
    s_tail = jet_rule_difference(s_tail, jet_rule_scaled(matching_rule_times(h, matching_rule_times(c_c, matching_rule_times(two_d, matching_rule_times(inner, z_x)))),
                                                          sim_rational(2ll, 1ll)));
    // (1 - h) sqrt 2 c X^(-h) - d/dX (sqrt 2 c X^(1-h)), and 2 X F_ext - sqrt 2 c (2d)^(-1-h) X w
    const JetRule root_two_c = matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, 1ll, 2ll, 0ll, 1ll), amplitude);
    const JetRule power_tail = jet_rule_difference(matching_rule_times(matching_rule_in_h(1ll, -1ll), matching_rule_times(root_two_c, jet_rule_power(JET_RULE_BASE_X, 0ll, 1ll, -1ll, 1ll))),
                                                   jet_rule_x(matching_rule_times(root_two_c, jet_rule_power(JET_RULE_BASE_X, 1ll, 1ll, -1ll, 1ll))));
    const JetRule h_field = jet_rule_difference(jet_rule_scaled(matching_rule_times(x, exterior), sim_rational(2ll, 1ll)),
                                                matching_rule_times(root_two_c, matching_rule_times(jet_rule_power(JET_RULE_BASE_TWO, -1ll, 1ll, -1ll, 1ll),
                                                                                                    matching_rule_times(jet_rule_power(JET_RULE_BASE_D, -1ll, 1ll, -1ll, 1ll), matching_rule_times(x, w)))));
    const int tails = jet_rule_zero(h_tail) && jet_rule_zero(s_tail) && jet_rule_zero(power_tail) && jet_rule_zero(h_field);
    matching_rule_report(&results, tails, "  the tails of H and S and the integral of H_pow hold with Kummer's equation",
                         "  a tail identity does not hold", "the tails");
    // the exterior's equation on X F_ext, and the tail of H with h - 1 taken as h - 2, are not 0
    const JetRule moved = matching_rule_times(x, exterior);
    const JetRule moved_outside = jet_rule_difference(matching_rule_t(moved, matching_rule_in_h(1ll, 1ll)),
                                                      jet_rule_scaled(jet_rule_x(jet_rule_x(matching_rule_times(x, moved))), sim_rational(2ll, 1ll)));
    const JetRule moved_tail = jet_rule_difference(h_tail, matching_rule_times(jet_rule_difference(matching_rule_times(z, w), z_power(0ll, -1ll)), z_x));
    matching_rule_report(&results, !jet_rule_zero(moved_outside) && !jet_rule_zero(moved_tail),
                         "  the exterior's equation on X F_ext and the tail of H with h - 2 are not 0",
                         "  a changed exterior or tail is still 0", "a changed exterior and tail seen");

    const int held = !jet_rule_short() && !run_cfg_short() && !record_short();
    matching_rule_report(&results, held, "  every exact value is held in the build's width",
                         "  a value outgrew the build's width or an exponent passed a word", "every exact value held");
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        const std::pair<const char *, const JetRule *> written[] = {
            {"X R_theta", &theta},          {"the torque's bracket G", &torque_bracket}, {"the torque's integrand q", &torque_local},
            {"the torque's g - G_X - q", &torque}, {"R_z", &along},                      {"the force's bracket G", &force_bracket},
            {"the force's integrand q", &force_local}, {"the force's g - G_X - q", &force}, {"F_ext", &exterior},
            {"the exterior's equation on F_ext", &outside}, {"the tail of H, its identity's derivative", &h_tail},
            {"the tail of S, its identity's derivative", &s_tail}};
        for (const auto &entry : written)
        {
            record_text(record, (std::string("expression ") + entry.first + ", " + std::to_string(jet_rule_terms(*entry.second)) + " terms").c_str());
            record_text(record, jet_rule_text(*entry.second).c_str());
        }
        recorded = record_close(record);
    }
    matching_rule_report(&results, recorded, "  every identity is written whole to the cfg's record",
                         "  the record is not written: the cfg names none, or it does not open", "record written");
    return sim_close(&results, "matching rule");
}
