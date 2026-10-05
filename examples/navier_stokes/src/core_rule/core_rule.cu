// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_rule.cu: the axis core of Duraiswami's matched core at every order, the system against the paper's rule at a
// general k (series_rule.h)
#include "run_cfg.h"

#include "record.h"
#include "series_rule.h"

// The system of OpenAI 2026, eq. (4.13), in the paper's profile variables with nu = 1, A = 1/2 + h, D = 1/2 - h,
// d = 1 - eta^2 and L = 1 - 2 h eta^2:
//     T_{-(A+1/2)} F + v0 (X F_X + F) + U Z_{-(A+1/2)} F - 2 (X F)_XX = 0,
//     T_{-A} U + X v0 U_X + U Z_{-A} U + Z_{-2A} Pi - 2 (X U_X)_X = 0,
//     (X v0)_X = L^-1 (2 A eta U - d U_eta + 2 eta X U_X),   Pi_X = F^2,
//     T_b f = L^-1 (-b f + D eta f_eta + X f_X),   Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X),
// its eqs. (3) and (4), and the rule they give at order k, the paper's eqs. (17) and (18) with the sign of the b term
// as (3) has it:
//     2 (k+1)(k+2) F_(k+1) = L^-1 ((k + 1 + h) F_k + D eta F_k') + sum v_i (k-i+1) F_(k-i) + sum U_i (Z_{-(A+1/2)} F)_(k-i),
//     2 (k+1)^2 U_(k+1) = L^-1 ((k + A) U_k + D eta U_k') + sum v_i (k-i) U_(k-i) + sum U_i (Z_{-A} U)_(k-i)
//                         + (Z_{-2A} Pi)_k,
//     (k+1) v_k = L^-1 (2 A eta U_k - d U_k' + 2 k eta U_k),   k P_k = sum_(i+j=k-1) F_i F_j,
// with (Z_b f)_j = L^-1 (2 b eta f_j + d f_j' - 2 j eta f_j). The system's side is taken by the operations of
// series_rule.h on the fields' series, the rule's side written part by part as the paper gives it.
// Checks:
// 1. d/dX (f g) - f' g - f g', X d/dX (f g) - X f' g - f X g', d/deta (f g) - f_eta g - f g_eta and
//    d/deta (L^-1 f) - L^-1 f_eta - 4 h eta L^-2 f are 0 at a general k.
// 2. The coefficient of X^k of each of the four equations equals the rule's at a general k, and with one part of the
//    rule changed, a single entry or a Cauchy product, it does not.
// 3. With F, v0 and Pi even in eta and U odd at every order to k, the rule's F_(k+1), v_(k+1) and P_(k+1) are even and
//    U_(k+1) odd: data F_0 and Pi_0 even and U_0 odd give the parities at every order.
// 4. The paper's eqs. (17) and (18) as printed hold L^-1 (-(A + 1/2) F_k + D eta F_k' + k F_k) and
//    L^-1 (-A U_k + D eta U_k' + k U_k), and its own T_b of (3) gives -b = +(A + 1/2) and +A: the printed parts differ
//    from T_{-(A+1/2)} F and T_{-A} U at order k by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k. The rule of 2 is
//    what (3) and (4) give.
// 5. Every exact value is held in the build's width.
// 6. Every expression is written whole to the cfg's record.
// The request: core_rule <cfg>.
//     bash examples/navier_stokes/run.sh core_rule examples/navier_stokes/cfg/core_rule.cfg

// the fields
enum
{
    CORE_RULE_ANGULAR = 0,
    CORE_RULE_AXIAL = 1,
    CORE_RULE_INFLOW = 2,
    CORE_RULE_PRESSURE = 3,
    CORE_RULE_FIRST = 4,
    CORE_RULE_SECOND = 5
};

static SeriesRulePolynomial core_rule_number(long long numerator, long long denominator)
{
    return series_rule_constant(sim_rational(numerator, denominator));
}

// a + b h
static SeriesRulePolynomial core_rule_in_h(SimRational whole, SimRational part)
{
    return series_rule_polynomial_sum(series_rule_constant(whole),
                                      series_rule_polynomial_product(series_rule_constant(part),
                                                                     series_rule_variable(SERIES_RULE_ANISOTROPY)));
}

// a + b k
static SeriesRulePolynomial core_rule_in_k(SeriesRulePolynomial whole, long long part)
{
    return series_rule_polynomial_sum(whole, series_rule_polynomial_product(core_rule_number(part, 1ll),
                                                                            series_rule_variable(SERIES_RULE_K)));
}

static SeriesRulePolynomial core_rule_times(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right)
{
    return series_rule_polynomial_product(left, right);
}

static SeriesRulePolynomial core_rule_eta(void)
{
    return series_rule_variable(SERIES_RULE_ETA);
}

// d = 1 - eta^2
static SeriesRulePolynomial core_rule_end(void)
{
    return series_rule_polynomial_difference(core_rule_number(1ll, 1ll), core_rule_times(core_rule_eta(), core_rule_eta()));
}

// D = 1/2 - h
static SeriesRulePolynomial core_rule_d(void)
{
    return core_rule_in_h(sim_rational(1ll, 2ll), sim_rational(-1ll, 1ll));
}

// L^-1 f
static SeriesRule core_rule_over_l(const SeriesRule &rule)
{
    return series_rule_scaled(rule, core_rule_number(1ll, 1ll), 1u);
}

// T_b f = L^-1 (-b f + D eta f_eta + X f_X), -b given
static SeriesRule core_rule_t(const SeriesRule &field, const SeriesRulePolynomial &minus_b)
{
    SeriesRule inner = series_rule_scaled(field, minus_b, 0u);
    inner = series_rule_sum(inner, series_rule_scaled(series_rule_slope_eta(field), core_rule_times(core_rule_d(), core_rule_eta()), 0u));
    inner = series_rule_sum(inner, series_rule_euler(field));
    return core_rule_over_l(inner);
}

// Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X), 2 b given
static SeriesRule core_rule_z(const SeriesRule &field, const SeriesRulePolynomial &two_b)
{
    SeriesRule inner = series_rule_scaled(field, core_rule_times(two_b, core_rule_eta()), 0u);
    inner = series_rule_sum(inner, series_rule_scaled(series_rule_slope_eta(field), core_rule_end(), 0u));
    inner = series_rule_sum(inner, series_rule_scaled(series_rule_euler(field), core_rule_times(core_rule_number(-2ll, 1ll), core_rule_eta()), 0u));
    return core_rule_over_l(inner);
}

// sum_i U_i (Z_b f)_(k-i) as the paper writes it: L^-1 ((2 b - 2 (k - i)) eta U_i f_(k-i) + d U_i f'_(k-i))
static SeriesRule core_rule_z_sum(unsigned int field, const SeriesRulePolynomial &two_b)
{
    const SeriesRulePolynomial order = series_rule_polynomial_difference(series_rule_variable(SERIES_RULE_K),
                                                                         series_rule_variable(SERIES_RULE_I));
    const SeriesRulePolynomial factor =
        core_rule_times(series_rule_polynomial_difference(two_b, core_rule_times(core_rule_number(2ll, 1ll), order)), core_rule_eta());
    return series_rule_sum(series_rule_cauchy(factor, 1u, CORE_RULE_AXIAL, 0u, field, 0u, 0),
                           series_rule_cauchy(core_rule_end(), 1u, CORE_RULE_AXIAL, 0u, field, 1u, 0));
}

// one check's line and its result
static int core_rule_report(SimResults *results, int passed, const char *pass, const char *fail, const char *what)
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
        fprintf(stderr, "core_rule <cfg>\n");
        return 2;
    }
    const std::vector<std::string> names = {"F", "U", "v", "P", "f", "g"};
    const SeriesRule angular = series_rule_entry(CORE_RULE_ANGULAR);
    const SeriesRule axial = series_rule_entry(CORE_RULE_AXIAL);
    const SeriesRule inflow = series_rule_entry(CORE_RULE_INFLOW);
    const SeriesRule pressure = series_rule_entry(CORE_RULE_PRESSURE);
    const SeriesRule first = series_rule_entry(CORE_RULE_FIRST);
    const SeriesRule second = series_rule_entry(CORE_RULE_SECOND);
    const SeriesRulePolynomial h = series_rule_variable(SERIES_RULE_ANISOTROPY);
    const SeriesRulePolynomial k = series_rule_variable(SERIES_RULE_K);

    // 1. the operations
    const SeriesRule both = series_rule_product(first, second);
    SeriesRule rule = series_rule_difference(series_rule_slope_x(both), series_rule_product(series_rule_slope_x(first), second));
    rule = series_rule_difference(rule, series_rule_product(first, series_rule_slope_x(second)));
    int operations = series_rule_zero(rule);
    rule = series_rule_difference(series_rule_euler(both), series_rule_product(series_rule_euler(first), second));
    rule = series_rule_difference(rule, series_rule_product(first, series_rule_euler(second)));
    operations = operations && series_rule_zero(rule);
    rule = series_rule_difference(series_rule_slope_eta(both), series_rule_product(series_rule_slope_eta(first), second));
    rule = series_rule_difference(rule, series_rule_product(first, series_rule_slope_eta(second)));
    operations = operations && series_rule_zero(rule);
    rule = series_rule_difference(series_rule_slope_eta(core_rule_over_l(first)), core_rule_over_l(series_rule_slope_eta(first)));
    rule = series_rule_difference(rule, series_rule_scaled(first, core_rule_times(core_rule_number(4ll, 1ll), core_rule_times(h, core_rule_eta())), 2u));
    operations = operations && series_rule_zero(rule);
    core_rule_report(&results, operations, "  the product rules in X and in eta and d/deta of L^-1 hold at a general k",
                     "  an operation fails its rule at a general k", "the operations");

    // 2. the four equations, the system's side
    const SeriesRulePolynomial one_h = core_rule_in_h(sim_rational(1ll, 1ll), sim_rational(1ll, 1ll));
    const SeriesRulePolynomial a = core_rule_in_h(sim_rational(1ll, 2ll), sim_rational(1ll, 1ll));
    const SeriesRulePolynomial theta_b = core_rule_in_h(sim_rational(-2ll, 1ll), sim_rational(-2ll, 1ll));
    const SeriesRulePolynomial axial_b = core_rule_in_h(sim_rational(-1ll, 1ll), sim_rational(-2ll, 1ll));
    const SeriesRulePolynomial pressure_b = core_rule_in_h(sim_rational(-2ll, 1ll), sim_rational(-4ll, 1ll));
    SeriesRule theta = core_rule_t(angular, one_h);
    theta = series_rule_sum(theta, series_rule_product(inflow, series_rule_sum(series_rule_euler(angular), angular)));
    theta = series_rule_sum(theta, series_rule_product(axial, core_rule_z(angular, theta_b)));
    theta = series_rule_difference(
        theta, series_rule_scaled(series_rule_slope_x(series_rule_slope_x(series_rule_times_x(angular))), core_rule_number(2ll, 1ll), 0u));
    SeriesRule along = core_rule_t(axial, a);
    along = series_rule_sum(along, series_rule_product(inflow, series_rule_euler(axial)));
    along = series_rule_sum(along, series_rule_product(axial, core_rule_z(axial, axial_b)));
    along = series_rule_sum(along, core_rule_z(pressure, pressure_b));
    along = series_rule_difference(along, series_rule_scaled(series_rule_slope_x(series_rule_euler(axial)), core_rule_number(2ll, 1ll), 0u));
    SeriesRule flow = series_rule_scaled(axial, core_rule_times(core_rule_times(core_rule_number(2ll, 1ll), a), core_rule_eta()), 0u);
    flow = series_rule_difference(flow, series_rule_scaled(series_rule_slope_eta(axial), core_rule_end(), 0u));
    flow = series_rule_sum(flow, series_rule_scaled(series_rule_euler(axial), core_rule_times(core_rule_number(2ll, 1ll), core_rule_eta()), 0u));
    flow = series_rule_difference(series_rule_slope_x(series_rule_times_x(inflow)), core_rule_over_l(flow));
    const SeriesRule rise = series_rule_difference(series_rule_slope_x(pressure), series_rule_product(angular, angular));

    // the rule's side, each equation as its left side less its right
    const SeriesRulePolynomial order = series_rule_polynomial_difference(k, series_rule_variable(SERIES_RULE_I));
    SeriesRule theta_right = series_rule_sum(series_rule_part(core_rule_in_k(one_h, 1ll), 1u, CORE_RULE_ANGULAR, 0, 0u),
                                  series_rule_part(core_rule_times(core_rule_d(), core_rule_eta()), 1u, CORE_RULE_ANGULAR, 0, 1u));
    theta_right = series_rule_sum(theta_right, series_rule_cauchy(core_rule_in_k(order, 0ll), 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_ANGULAR, 0u, 0));
    theta_right = series_rule_sum(theta_right, series_rule_cauchy(core_rule_number(1ll, 1ll), 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_ANGULAR, 0u, 0));
    theta_right = series_rule_sum(theta_right, core_rule_z_sum(CORE_RULE_ANGULAR, theta_b));
    const SeriesRulePolynomial theta_left = core_rule_times(core_rule_number(2ll, 1ll),
                                                            core_rule_times(core_rule_in_k(core_rule_number(1ll, 1ll), 1ll),
                                                                            core_rule_in_k(core_rule_number(2ll, 1ll), 1ll)));
    const SeriesRule theta_rule = series_rule_difference(series_rule_part(theta_left, 0u, CORE_RULE_ANGULAR, 1, 0u), theta_right);

    SeriesRule along_right = series_rule_sum(series_rule_part(series_rule_polynomial_sum(k, a), 1u, CORE_RULE_AXIAL, 0, 0u),
                                             series_rule_part(core_rule_times(core_rule_d(), core_rule_eta()), 1u, CORE_RULE_AXIAL, 0, 1u));
    along_right = series_rule_sum(along_right, series_rule_cauchy(order, 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_AXIAL, 0u, 0));
    along_right = series_rule_sum(along_right, core_rule_z_sum(CORE_RULE_AXIAL, axial_b));
    // (Z_{-2A} Pi)_k = L^-1 ((2 b - 2 k) eta P_k + d P_k')
    along_right = series_rule_sum(along_right, series_rule_part(core_rule_times(series_rule_polynomial_difference(pressure_b, core_rule_times(core_rule_number(2ll, 1ll), k)), core_rule_eta()),
                                                                1u, CORE_RULE_PRESSURE, 0, 0u));
    along_right = series_rule_sum(along_right, series_rule_part(core_rule_end(), 1u, CORE_RULE_PRESSURE, 0, 1u));
    const SeriesRulePolynomial along_left = core_rule_times(core_rule_number(2ll, 1ll),
                                                            core_rule_times(core_rule_in_k(core_rule_number(1ll, 1ll), 1ll),
                                                                            core_rule_in_k(core_rule_number(1ll, 1ll), 1ll)));
    const SeriesRule along_rule = series_rule_difference(series_rule_part(along_left, 0u, CORE_RULE_AXIAL, 1, 0u), along_right);

    SeriesRule flow_right = series_rule_part(core_rule_times(core_rule_times(core_rule_number(2ll, 1ll), a), core_rule_eta()), 1u, CORE_RULE_AXIAL, 0, 0u);
    flow_right = series_rule_difference(flow_right, series_rule_part(core_rule_end(), 1u, CORE_RULE_AXIAL, 0, 1u));
    flow_right = series_rule_sum(flow_right, series_rule_part(core_rule_times(core_rule_times(core_rule_number(2ll, 1ll), k), core_rule_eta()), 1u, CORE_RULE_AXIAL, 0, 0u));
    const SeriesRule flow_rule =
        series_rule_difference(series_rule_part(core_rule_in_k(core_rule_number(1ll, 1ll), 1ll), 0u, CORE_RULE_INFLOW, 0, 0u), flow_right);

    // k P_k = sum_(i+j=k-1) F_i F_j, against the system's entry at k - 1
    const SeriesRule rise_right = series_rule_cauchy(core_rule_number(1ll, 1ll), 0u, CORE_RULE_ANGULAR, 0u, CORE_RULE_ANGULAR, 0u, -1);
    const SeriesRule rise_rule = series_rule_difference(series_rule_part(k, 0u, CORE_RULE_PRESSURE, 0, 0u), rise_right);

    const int theta_equal = series_rule_zero(series_rule_sum(theta, theta_rule));
    const int along_equal = series_rule_zero(series_rule_sum(along, along_rule));
    const int flow_equal = series_rule_zero(series_rule_difference(flow, flow_rule));
    const int rise_equal = series_rule_zero(series_rule_difference(series_rule_shift(rise, -1), rise_rule));
    scriptura_text(&results.line, "  the coefficient of X^k against the rule at a general k: theta ");
    scriptura_text(&results.line, theta_equal ? "equal" : "differs");
    scriptura_text(&results.line, ", z ");
    scriptura_text(&results.line, along_equal ? "equal" : "differs");
    scriptura_text(&results.line, ", v0 ");
    scriptura_text(&results.line, flow_equal ? "equal" : "differs");
    scriptura_text(&results.line, ", Pi ");
    scriptura_text(&results.line, rise_equal ? "equal" : "differs");
    scriptura_character(&results.line, '\n');
    sim_flush(&results);
    sim_check(&results, theta_equal && along_equal && flow_equal && rise_equal, "the system against the rule at a general k");
    // the same comparison with one part of the rule changed, a single entry and a Cauchy product, is not 0
    const SeriesRule changed_entry = series_rule_sum(theta_rule, series_rule_part(k, 0u, CORE_RULE_ANGULAR, 1, 0u));
    const SeriesRule changed_product = series_rule_sum(along_rule, series_rule_cauchy(order, 1u, CORE_RULE_AXIAL, 0u, CORE_RULE_AXIAL, 0u, 0));
    const int seen = !series_rule_zero(series_rule_sum(theta, changed_entry)) && !series_rule_zero(series_rule_sum(along, changed_product));
    core_rule_report(&results, seen, "  the comparison with one part of the rule changed is not 0",
                     "  the comparison does not see a changed part", "a changed part seen");

    // 4. the paper's eqs. (17) and (18) as printed: L^-1 (-(A + 1/2) F_k + D eta F_k' + k F_k) and
    // L^-1 (-A U_k + D eta U_k' + k U_k), against T_{-(A+1/2)} F and T_{-A} U of its (3) at order k
    const SeriesRule printed_theta = series_rule_sum(
        series_rule_part(series_rule_polynomial_difference(k, one_h), 1u, CORE_RULE_ANGULAR, 0, 0u),
        series_rule_part(core_rule_times(core_rule_d(), core_rule_eta()), 1u, CORE_RULE_ANGULAR, 0, 1u));
    const SeriesRule printed_along = series_rule_sum(
        series_rule_part(series_rule_polynomial_difference(k, a), 1u, CORE_RULE_AXIAL, 0, 0u),
        series_rule_part(core_rule_times(core_rule_d(), core_rule_eta()), 1u, CORE_RULE_AXIAL, 0, 1u));
    const SeriesRule theta_gap = series_rule_difference(core_rule_t(angular, one_h), printed_theta);
    const SeriesRule along_gap = series_rule_difference(core_rule_t(axial, a), printed_along);
    const int printed = !series_rule_zero(theta_gap) && !series_rule_zero(along_gap) &&
                        series_rule_zero(series_rule_difference(theta_gap, series_rule_part(series_rule_polynomial_sum(one_h, one_h), 1u, CORE_RULE_ANGULAR, 0, 0u))) &&
                        series_rule_zero(series_rule_difference(along_gap, series_rule_part(series_rule_polynomial_sum(a, a), 1u, CORE_RULE_AXIAL, 0, 0u)));
    core_rule_report(&results, printed,
                     "  the paper's printed (17) and (18) differ from its (3) and (4) by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k",
                     "  the paper's printed (17) and (18) differ from its (3) and (4) otherwise", "the printed rule against the system");

    // 3. the parities carried one order
    const std::vector<int> parities = {1, -1, 1, 1, 1, 1};
    const int carried = (series_rule_parity(theta_right, parities) == 1) && (series_rule_parity(along_right, parities) == -1) &&
                        (series_rule_parity(flow_right, parities) == 1) && (series_rule_parity(rise_right, parities) == 1);
    core_rule_report(&results, carried, "  F, v0 and Pi even and U odd at every order to k give them at the next order",
                     "  the rule does not carry the parities one order", "the parities at every order");

    // 4. and 5.
    const int held = !series_rule_short() && !run_cfg_short() && !record_short();
    core_rule_report(&results, held, "  every exact value is held in the build's width",
                     "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS", "every exact value held");
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        const std::pair<const char *, const SeriesRule *> written[] = {
            {"theta, the system's coefficient of X^k", &theta}, {"theta, the rule's left side less its right", &theta_rule},
            {"z, the system's coefficient of X^k", &along},      {"z, the rule's left side less its right", &along_rule},
            {"v0, the system's coefficient of X^k", &flow},      {"v0, the rule's left side less its right", &flow_rule},
            {"Pi, the system's coefficient of X^k", &rise},      {"Pi, the rule's left side less its right", &rise_rule}};
        for (const auto &entry : written)
        {
            record_text(record, (std::string("expression ") + entry.first + ", " + std::to_string(series_rule_parts(*entry.second)) + " parts").c_str());
            record_text(record, series_rule_text(*entry.second, names).c_str());
        }
        recorded = record_close(record);
    }
    core_rule_report(&results, recorded, "  every expression is written whole to the cfg's record",
                     "  the record is not written: the cfg names none, or it does not open", "record written");
    return sim_close(&results, "core rule");
}
