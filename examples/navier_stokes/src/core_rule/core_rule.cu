// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_rule.cu: the axis core of Duraiswami's matched core at every order, the system against the paper's rule at a
// general k (series_rule.h)
#include "run_cfg.h"

#include "record.h"
#include "series_rule.h"
#include "../../../../includes/platform/platform.h"

// The system of OpenAI 2026, eq. (4.13), in the paper's profile variables with nu = 1, A = 1/2 + h, D = 1/2 - h,
// d = 1 - eta^2 and L = 1 - 2 h eta^2:
//     T_{-(A+1/2)} F + v0 (X F_X + F) + U Z_{-(A+1/2)} F - 2 (X F)_XX = 0,
//     T_{-A} U + X v0 U_X + U Z_{-A} U + Z_{-2A} Pi - 2 (X U_X)_X = 0,
//     (X v0)_X = L^-1 (2 A eta U - d U_eta + 2 eta X U_X),   Pi_X = F^2,
//     T_b f = L^-1 (-b f + D eta f_eta + X f_X),   Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X),
// its eqs. (3) and (4), and the rule they give at order k, the paper's eqs. (17) and (18) with the sign of the b term
// as (3) has it:
//     2 (k+1)(k+2) F_(k+1) = L^-1 ((k + 1 + h) F_k + D eta F_k') + sum v_i (k-i+1) F_(k-i) + sum U_i (Z_{-(A+1/2)}
//     F)_(k-i), 2 (k+1)^2 U_(k+1) = L^-1 ((k + A) U_k + D eta U_k') + sum v_i (k-i) U_(k-i) + sum U_i (Z_{-A} U)_(k-i)
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
// 4. The paper's eqs. (17) and (18) as printed satisfy L^-1 (-(A + 1/2) F_k + D eta F_k' + k F_k) and
//    L^-1 (-A U_k + D eta U_k' + k U_k), and its own T_b of (3) gives -b = +(A + 1/2) and +A: the printed parts differ
//    from T_{-(A+1/2)} F and T_{-A} U at order k by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k. The rule of 2 is
//    what (3) and (4) give.
// 5. Every exact value is stored in the build's width.
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

// Each call below writes its polynomial or its expression into `out`, which may be one of its operands, and gives back
// every cell it takes for itself before it returns (series_rule.h).

static void core_rule_number(long long numerator, long long denominator, SeriesRulePolynomial *out)
{
    series_rule_constant(sim_rational(numerator, denominator), out);
}

// a + b h
static void core_rule_in_h(SimRational whole, SimRational part, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial constant = SERIES_RULE_NONE;
    SeriesRulePolynomial h = SERIES_RULE_NONE;
    SeriesRulePolynomial times = SERIES_RULE_NONE;
    series_rule_constant(part, &constant);
    series_rule_variable(SERIES_RULE_ANISOTROPY, &h);
    series_rule_polynomial_product(&constant, &h, &times);
    series_rule_constant(whole, &constant);
    series_rule_polynomial_sum(&constant, &times, out);
    series_rule_polynomial_released(&constant);
    series_rule_polynomial_released(&h);
    series_rule_polynomial_released(&times);
}

// a + b k
static void core_rule_in_k(const SeriesRulePolynomial *whole, long long part, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial number = SERIES_RULE_NONE;
    SeriesRulePolynomial k = SERIES_RULE_NONE;
    core_rule_number(part, 1ll, &number);
    series_rule_variable(SERIES_RULE_K, &k);
    series_rule_polynomial_product(&number, &k, &k);
    series_rule_polynomial_sum(whole, &k, out);
    series_rule_polynomial_released(&number);
    series_rule_polynomial_released(&k);
}

// c times eta
static void core_rule_eta_times(const SeriesRulePolynomial *factor, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial eta = SERIES_RULE_NONE;
    series_rule_variable(SERIES_RULE_ETA, &eta);
    series_rule_polynomial_product(factor, &eta, out);
    series_rule_polynomial_released(&eta);
}

// numerator / denominator times eta
static void core_rule_number_eta(long long numerator, long long denominator, SeriesRulePolynomial *out)
{
    SeriesRulePolynomial number = SERIES_RULE_NONE;
    core_rule_number(numerator, denominator, &number);
    core_rule_eta_times(&number, out);
    series_rule_polynomial_released(&number);
}

// d = 1 - eta^2
static void core_rule_end(SeriesRulePolynomial *out)
{
    SeriesRulePolynomial one = SERIES_RULE_NONE;
    SeriesRulePolynomial eta = SERIES_RULE_NONE;
    core_rule_number(1ll, 1ll, &one);
    series_rule_variable(SERIES_RULE_ETA, &eta);
    series_rule_polynomial_product(&eta, &eta, &eta);
    series_rule_polynomial_difference(&one, &eta, out);
    series_rule_polynomial_released(&one);
    series_rule_polynomial_released(&eta);
}

// D = 1/2 - h
static void core_rule_d(SeriesRulePolynomial *out)
{
    core_rule_in_h(sim_rational(1ll, 2ll), sim_rational(-1ll, 1ll), out);
}

// D eta
static void core_rule_d_eta(SeriesRulePolynomial *out)
{
    core_rule_d(out);
    core_rule_eta_times(out, out);
}

// L^-1 f
static void core_rule_over_l(const SeriesRule *rule, SeriesRule *out)
{
    SeriesRulePolynomial one = SERIES_RULE_NONE;
    core_rule_number(1ll, 1ll, &one);
    series_rule_scaled(rule, &one, 1u, out);
    series_rule_polynomial_released(&one);
}

// `rule` taken by `operation` and times `factor`, summed into `into`
static void core_rule_sum_into(SeriesRule *into, const SeriesRule *rule,
                               void (*operation)(const SeriesRule *, SeriesRule *), const SeriesRulePolynomial *factor)
{
    SeriesRule taken = SERIES_RULE_NONE;
    operation(rule, &taken);
    if (factor != NULL)
    {
        series_rule_scaled(&taken, factor, 0u, &taken);
    }
    series_rule_sum(into, &taken, into);
    series_rule_released(&taken);
}

// T_b f = L^-1 (-b f + D eta f_eta + X f_X), -b given
static void core_rule_t(const SeriesRule *field, const SeriesRulePolynomial *minus_b, SeriesRule *out)
{
    SeriesRule inner = SERIES_RULE_NONE;
    SeriesRulePolynomial d_eta = SERIES_RULE_NONE;
    series_rule_scaled(field, minus_b, 0u, &inner);
    core_rule_d_eta(&d_eta);
    core_rule_sum_into(&inner, field, series_rule_slope_eta, &d_eta);
    core_rule_sum_into(&inner, field, series_rule_euler, NULL);
    core_rule_over_l(&inner, out);
    series_rule_released(&inner);
    series_rule_polynomial_released(&d_eta);
}

// Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X), 2 b given
static void core_rule_z(const SeriesRule *field, const SeriesRulePolynomial *two_b, SeriesRule *out)
{
    SeriesRule inner = SERIES_RULE_NONE;
    SeriesRulePolynomial factor = SERIES_RULE_NONE;
    core_rule_eta_times(two_b, &factor);
    series_rule_scaled(field, &factor, 0u, &inner);
    core_rule_end(&factor);
    core_rule_sum_into(&inner, field, series_rule_slope_eta, &factor);
    core_rule_number_eta(-2ll, 1ll, &factor);
    core_rule_sum_into(&inner, field, series_rule_euler, &factor);
    core_rule_over_l(&inner, out);
    series_rule_released(&inner);
    series_rule_polynomial_released(&factor);
}

// k - i
static void core_rule_order(SeriesRulePolynomial *out)
{
    SeriesRulePolynomial k = SERIES_RULE_NONE;
    SeriesRulePolynomial i = SERIES_RULE_NONE;
    series_rule_variable(SERIES_RULE_K, &k);
    series_rule_variable(SERIES_RULE_I, &i);
    series_rule_polynomial_difference(&k, &i, out);
    series_rule_polynomial_released(&k);
    series_rule_polynomial_released(&i);
}

// sum_i U_i (Z_b f)_(k-i) as the paper writes it: L^-1 ((2 b - 2 (k - i)) eta U_i f_(k-i) + d U_i f'_(k-i))
static void core_rule_z_sum(unsigned int field, const SeriesRulePolynomial *two_b, SeriesRule *out)
{
    SeriesRulePolynomial order = SERIES_RULE_NONE;
    SeriesRulePolynomial two = SERIES_RULE_NONE;
    SeriesRulePolynomial factor = SERIES_RULE_NONE;
    core_rule_order(&order);
    core_rule_number(2ll, 1ll, &two);
    series_rule_polynomial_product(&two, &order, &factor);
    series_rule_polynomial_difference(two_b, &factor, &factor);
    core_rule_eta_times(&factor, &factor);
    SeriesRule eta_part = SERIES_RULE_NONE;
    SeriesRule end_part = SERIES_RULE_NONE;
    series_rule_cauchy(&factor, 1u, CORE_RULE_AXIAL, 0u, field, 0u, 0, &eta_part);
    core_rule_end(&factor);
    series_rule_cauchy(&factor, 1u, CORE_RULE_AXIAL, 0u, field, 1u, 0, &end_part);
    series_rule_sum(&eta_part, &end_part, out);
    series_rule_released(&eta_part);
    series_rule_released(&end_part);
    series_rule_polynomial_released(&order);
    series_rule_polynomial_released(&two);
    series_rule_polynomial_released(&factor);
}

// `out` less the part `factor` L^-power times the entry `field` at k + shift of slope `slope`, or plus it at `sign` 1
static void core_rule_part_into(SeriesRule *out, int sign, const SeriesRulePolynomial *factor, unsigned int power,
                                unsigned int field, int shift, unsigned int slope)
{
    SeriesRule part = SERIES_RULE_NONE;
    series_rule_part(factor, power, field, shift, slope, &part);
    if (sign > 0)
    {
        series_rule_sum(out, &part, out);
    }
    else
    {
        series_rule_difference(out, &part, out);
    }
    series_rule_released(&part);
}

// the Cauchy product `factor` L^-power (first at i) (second at k + shift - i) summed into `out`
static void core_rule_cauchy_into(SeriesRule *out, const SeriesRulePolynomial *factor, unsigned int power,
                                  unsigned int first, unsigned int first_slope, unsigned int second,
                                  unsigned int second_slope, int shift)
{
    SeriesRule part = SERIES_RULE_NONE;
    series_rule_cauchy(factor, power, first, first_slope, second, second_slope, shift, &part);
    series_rule_sum(out, &part, out);
    series_rule_released(&part);
}

// 1 where `operation` of the product of `first` and `second` is its rule: op(f g) - op(f) g - f op(g) is 0
static int core_rule_product_rule(void (*operation)(const SeriesRule *, SeriesRule *), const SeriesRule *first,
                                  const SeriesRule *second, const SeriesRule *both)
{
    SeriesRule rule = SERIES_RULE_NONE;
    SeriesRule taken = SERIES_RULE_NONE;
    SeriesRule product = SERIES_RULE_NONE;
    operation(both, &rule);
    operation(first, &taken);
    series_rule_product(&taken, second, &product);
    series_rule_difference(&rule, &product, &rule);
    operation(second, &taken);
    series_rule_product(first, &taken, &product);
    series_rule_difference(&rule, &product, &rule);
    const int zero = series_rule_zero(&rule);
    series_rule_released(&rule);
    series_rule_released(&taken);
    series_rule_released(&product);
    return zero;
}

// 1 where `left` plus `right`, or less it at `sign` -1, is 0
static int core_rule_equal(const SeriesRule *left, const SeriesRule *right, int sign)
{
    SeriesRule rule = SERIES_RULE_NONE;
    if (sign > 0)
    {
        series_rule_sum(left, right, &rule);
    }
    else
    {
        series_rule_difference(left, right, &rule);
    }
    const int zero = series_rule_zero(&rule);
    series_rule_released(&rule);
    return zero;
}

// one check's line and its result
static int core_rule_report(SimResults *results, int passed, const char *pass, const char *fail, const char *what)
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
        fprintf(stderr, "core_rule <cfg>\n");
        return 2;
    }
    static const char *const names[6] = {"F", "U", "v", "P", "f", "g"};
    // Every expression and polynomial of the run, each given back as the run ends
    enum
    {
        ANGULAR,
        AXIAL,
        INFLOW,
        PRESSURE,
        FIRST,
        SECOND,
        BOTH,
        RULE,
        PART,
        OTHER,
        THETA,
        ALONG,
        FLOW,
        RISE,
        THETA_RIGHT,
        THETA_RULE,
        ALONG_RIGHT,
        ALONG_RULE,
        FLOW_RIGHT,
        FLOW_RULE,
        RISE_RIGHT,
        RISE_RULE,
        RULES
    };
    enum
    {
        H,
        K,
        FACTOR,
        SECOND_FACTOR,
        ONE_H,
        A,
        THETA_B,
        AXIAL_B,
        PRESSURE_B,
        ORDER,
        POLYNOMIALS
    };
    static SeriesRule s_rule[RULES];
    static SeriesRulePolynomial s_polynomial[POLYNOMIALS];
    for (unsigned int each = 0u; each < RULES; each += 1u)
    {
        s_rule[each] = SeriesRule SERIES_RULE_NONE;
    }
    for (unsigned int each = 0u; each < POLYNOMIALS; each += 1u)
    {
        s_polynomial[each] = SeriesRulePolynomial SERIES_RULE_NONE;
    }
    SeriesRule *const angular = &s_rule[ANGULAR];
    SeriesRule *const axial = &s_rule[AXIAL];
    SeriesRule *const inflow = &s_rule[INFLOW];
    SeriesRule *const pressure = &s_rule[PRESSURE];
    SeriesRule *const first = &s_rule[FIRST];
    SeriesRule *const second = &s_rule[SECOND];
    SeriesRule *const both = &s_rule[BOTH];
    SeriesRule *const rule = &s_rule[RULE];
    SeriesRule *const part = &s_rule[PART];
    SeriesRule *const other = &s_rule[OTHER];
    SeriesRulePolynomial *const h = &s_polynomial[H];
    SeriesRulePolynomial *const k = &s_polynomial[K];
    SeriesRulePolynomial *const factor = &s_polynomial[FACTOR];
    SeriesRulePolynomial *const second_factor = &s_polynomial[SECOND_FACTOR];
    series_rule_entry(CORE_RULE_ANGULAR, angular);
    series_rule_entry(CORE_RULE_AXIAL, axial);
    series_rule_entry(CORE_RULE_INFLOW, inflow);
    series_rule_entry(CORE_RULE_PRESSURE, pressure);
    series_rule_entry(CORE_RULE_FIRST, first);
    series_rule_entry(CORE_RULE_SECOND, second);
    series_rule_variable(SERIES_RULE_ANISOTROPY, h);
    series_rule_variable(SERIES_RULE_K, k);

    // 1. the operations
    series_rule_product(first, second, both);
    int operations = core_rule_product_rule(series_rule_slope_x, first, second, both);
    operations = operations && core_rule_product_rule(series_rule_euler, first, second, both);
    operations = operations && core_rule_product_rule(series_rule_slope_eta, first, second, both);
    core_rule_over_l(first, part);
    series_rule_slope_eta(part, rule);
    series_rule_slope_eta(first, part);
    core_rule_over_l(part, part);
    series_rule_difference(rule, part, rule);
    core_rule_number_eta(4ll, 1ll, factor);
    series_rule_polynomial_product(factor, h, factor);
    series_rule_scaled(first, factor, 2u, part);
    series_rule_difference(rule, part, rule);
    operations = operations && series_rule_zero(rule);
    core_rule_report(&results, operations,
                     "  the product rules in X and in eta and d/deta of L^-1 are valid at a general k",
                     "  an operation fails its rule at a general k", "the operations");

    // 2. the four equations, the system's side
    SeriesRulePolynomial *const one_h = &s_polynomial[ONE_H];
    SeriesRulePolynomial *const a = &s_polynomial[A];
    SeriesRulePolynomial *const theta_b = &s_polynomial[THETA_B];
    SeriesRulePolynomial *const axial_b = &s_polynomial[AXIAL_B];
    SeriesRulePolynomial *const pressure_b = &s_polynomial[PRESSURE_B];
    core_rule_in_h(sim_rational(1ll, 1ll), sim_rational(1ll, 1ll), one_h);
    core_rule_in_h(sim_rational(1ll, 2ll), sim_rational(1ll, 1ll), a);
    core_rule_in_h(sim_rational(-2ll, 1ll), sim_rational(-2ll, 1ll), theta_b);
    core_rule_in_h(sim_rational(-1ll, 1ll), sim_rational(-2ll, 1ll), axial_b);
    core_rule_in_h(sim_rational(-2ll, 1ll), sim_rational(-4ll, 1ll), pressure_b);
    SeriesRule *const theta = &s_rule[THETA];
    core_rule_t(angular, one_h, theta);
    series_rule_euler(angular, part);
    series_rule_sum(part, angular, part);
    series_rule_product(inflow, part, part);
    series_rule_sum(theta, part, theta);
    core_rule_z(angular, theta_b, part);
    series_rule_product(axial, part, part);
    series_rule_sum(theta, part, theta);
    series_rule_times_x(angular, part);
    series_rule_slope_x(part, part);
    series_rule_slope_x(part, part);
    core_rule_number(2ll, 1ll, factor);
    series_rule_scaled(part, factor, 0u, part);
    series_rule_difference(theta, part, theta);
    SeriesRule *const along = &s_rule[ALONG];
    core_rule_t(axial, a, along);
    series_rule_euler(axial, part);
    series_rule_product(inflow, part, part);
    series_rule_sum(along, part, along);
    core_rule_z(axial, axial_b, part);
    series_rule_product(axial, part, part);
    series_rule_sum(along, part, along);
    core_rule_z(pressure, pressure_b, part);
    series_rule_sum(along, part, along);
    series_rule_euler(axial, part);
    series_rule_slope_x(part, part);
    series_rule_scaled(part, factor, 0u, part);
    series_rule_difference(along, part, along);
    SeriesRule *const flow = &s_rule[FLOW];
    series_rule_polynomial_product(factor, a, second_factor);
    core_rule_eta_times(second_factor, second_factor);
    series_rule_scaled(axial, second_factor, 0u, flow);
    series_rule_slope_eta(axial, part);
    core_rule_end(second_factor);
    series_rule_scaled(part, second_factor, 0u, part);
    series_rule_difference(flow, part, flow);
    series_rule_euler(axial, part);
    core_rule_number_eta(2ll, 1ll, second_factor);
    series_rule_scaled(part, second_factor, 0u, part);
    series_rule_sum(flow, part, flow);
    core_rule_over_l(flow, other);
    series_rule_times_x(inflow, part);
    series_rule_slope_x(part, part);
    series_rule_difference(part, other, flow);
    SeriesRule *const rise = &s_rule[RISE];
    series_rule_slope_x(pressure, part);
    series_rule_product(angular, angular, other);
    series_rule_difference(part, other, rise);

    // the rule's side, each equation as its left side less its right
    SeriesRulePolynomial *const order = &s_polynomial[ORDER];
    core_rule_order(order);
    SeriesRule *const theta_right = &s_rule[THETA_RIGHT];
    core_rule_in_k(one_h, 1ll, factor);
    series_rule_part(factor, 1u, CORE_RULE_ANGULAR, 0, 0u, theta_right);
    core_rule_d_eta(factor);
    core_rule_part_into(theta_right, 1, factor, 1u, CORE_RULE_ANGULAR, 0, 1u);
    core_rule_in_k(order, 0ll, factor);
    core_rule_cauchy_into(theta_right, factor, 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_ANGULAR, 0u, 0);
    core_rule_number(1ll, 1ll, factor);
    core_rule_cauchy_into(theta_right, factor, 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_ANGULAR, 0u, 0);
    core_rule_z_sum(CORE_RULE_ANGULAR, theta_b, part);
    series_rule_sum(theta_right, part, theta_right);
    core_rule_number(1ll, 1ll, factor);
    core_rule_in_k(factor, 1ll, factor);
    core_rule_number(2ll, 1ll, second_factor);
    core_rule_in_k(second_factor, 1ll, second_factor);
    series_rule_polynomial_product(factor, second_factor, factor);
    core_rule_number(2ll, 1ll, second_factor);
    series_rule_polynomial_product(second_factor, factor, factor);
    SeriesRule *const theta_rule = &s_rule[THETA_RULE];
    series_rule_part(factor, 0u, CORE_RULE_ANGULAR, 1, 0u, part);
    series_rule_difference(part, theta_right, theta_rule);

    SeriesRule *const along_right = &s_rule[ALONG_RIGHT];
    series_rule_polynomial_sum(k, a, factor);
    series_rule_part(factor, 1u, CORE_RULE_AXIAL, 0, 0u, along_right);
    core_rule_d_eta(factor);
    core_rule_part_into(along_right, 1, factor, 1u, CORE_RULE_AXIAL, 0, 1u);
    core_rule_cauchy_into(along_right, order, 0u, CORE_RULE_INFLOW, 0u, CORE_RULE_AXIAL, 0u, 0);
    core_rule_z_sum(CORE_RULE_AXIAL, axial_b, part);
    series_rule_sum(along_right, part, along_right);
    // (Z_{-2A} Pi)_k = L^-1 ((2 b - 2 k) eta P_k + d P_k')
    core_rule_number(2ll, 1ll, factor);
    series_rule_polynomial_product(factor, k, factor);
    series_rule_polynomial_difference(pressure_b, factor, factor);
    core_rule_eta_times(factor, factor);
    core_rule_part_into(along_right, 1, factor, 1u, CORE_RULE_PRESSURE, 0, 0u);
    core_rule_end(factor);
    core_rule_part_into(along_right, 1, factor, 1u, CORE_RULE_PRESSURE, 0, 1u);
    core_rule_number(1ll, 1ll, factor);
    core_rule_in_k(factor, 1ll, factor);
    series_rule_polynomial_product(factor, factor, factor);
    core_rule_number(2ll, 1ll, second_factor);
    series_rule_polynomial_product(second_factor, factor, factor);
    SeriesRule *const along_rule = &s_rule[ALONG_RULE];
    series_rule_part(factor, 0u, CORE_RULE_AXIAL, 1, 0u, part);
    series_rule_difference(part, along_right, along_rule);

    SeriesRule *const flow_right = &s_rule[FLOW_RIGHT];
    core_rule_number(2ll, 1ll, factor);
    series_rule_polynomial_product(factor, a, factor);
    core_rule_eta_times(factor, factor);
    series_rule_part(factor, 1u, CORE_RULE_AXIAL, 0, 0u, flow_right);
    core_rule_end(factor);
    core_rule_part_into(flow_right, -1, factor, 1u, CORE_RULE_AXIAL, 0, 1u);
    core_rule_number(2ll, 1ll, factor);
    series_rule_polynomial_product(factor, k, factor);
    core_rule_eta_times(factor, factor);
    core_rule_part_into(flow_right, 1, factor, 1u, CORE_RULE_AXIAL, 0, 0u);
    SeriesRule *const flow_rule = &s_rule[FLOW_RULE];
    core_rule_number(1ll, 1ll, factor);
    core_rule_in_k(factor, 1ll, factor);
    series_rule_part(factor, 0u, CORE_RULE_INFLOW, 0, 0u, part);
    series_rule_difference(part, flow_right, flow_rule);

    // k P_k = sum_(i+j=k-1) F_i F_j, against the system's entry at k - 1
    SeriesRule *const rise_right = &s_rule[RISE_RIGHT];
    core_rule_number(1ll, 1ll, factor);
    series_rule_cauchy(factor, 0u, CORE_RULE_ANGULAR, 0u, CORE_RULE_ANGULAR, 0u, -1, rise_right);
    SeriesRule *const rise_rule = &s_rule[RISE_RULE];
    series_rule_part(k, 0u, CORE_RULE_PRESSURE, 0, 0u, part);
    series_rule_difference(part, rise_right, rise_rule);

    const int theta_equal = core_rule_equal(theta, theta_rule, 1);
    const int along_equal = core_rule_equal(along, along_rule, 1);
    const int flow_equal = core_rule_equal(flow, flow_rule, -1);
    series_rule_shift(rise, -1, part);
    const int rise_equal = core_rule_equal(part, rise_rule, -1);
    mmgr_line_put(&results.line, "  the coefficient of X^k against the rule at a general k: theta ");
    mmgr_line_put(&results.line, theta_equal ? "equal" : "differs");
    mmgr_line_put(&results.line, ", z ");
    mmgr_line_put(&results.line, along_equal ? "equal" : "differs");
    mmgr_line_put(&results.line, ", v0 ");
    mmgr_line_put(&results.line, flow_equal ? "equal" : "differs");
    mmgr_line_put(&results.line, ", Pi ");
    mmgr_line_put(&results.line, rise_equal ? "equal" : "differs");
    mmgr_line_ch(&results.line, '\n');
    sim_flush(&results);
    sim_check(&results, theta_equal && along_equal && flow_equal && rise_equal, "the system against the rule at a general k");
    // the same comparison with one part of the rule changed, a single entry and a Cauchy product, is not 0
    series_rule_part(k, 0u, CORE_RULE_ANGULAR, 1, 0u, part);
    series_rule_sum(theta_rule, part, part);
    series_rule_cauchy(order, 1u, CORE_RULE_AXIAL, 0u, CORE_RULE_AXIAL, 0u, 0, other);
    series_rule_sum(along_rule, other, other);
    const int seen = !core_rule_equal(theta, part, 1) && !core_rule_equal(along, other, 1);
    core_rule_report(&results, seen, "  the comparison with one part of the rule changed is not 0",
                     "  the comparison does not see a changed part", "a changed part seen");

    // 4. the paper's eqs. (17) and (18) as printed: L^-1 (-(A + 1/2) F_k + D eta F_k' + k F_k) and
    // L^-1 (-A U_k + D eta U_k' + k U_k), against T_{-(A+1/2)} F and T_{-A} U of its (3) at order k
    series_rule_polynomial_difference(k, one_h, factor);
    series_rule_part(factor, 1u, CORE_RULE_ANGULAR, 0, 0u, part);
    core_rule_d_eta(factor);
    core_rule_part_into(part, 1, factor, 1u, CORE_RULE_ANGULAR, 0, 1u);
    core_rule_t(angular, one_h, other);
    series_rule_difference(other, part, part);
    SeriesRule theta_gap = SERIES_RULE_NONE;
    theta_gap = *part;
    *part = SeriesRule SERIES_RULE_NONE;
    series_rule_polynomial_difference(k, a, factor);
    series_rule_part(factor, 1u, CORE_RULE_AXIAL, 0, 0u, part);
    core_rule_d_eta(factor);
    core_rule_part_into(part, 1, factor, 1u, CORE_RULE_AXIAL, 0, 1u);
    core_rule_t(axial, a, other);
    series_rule_difference(other, part, part);
    SeriesRule along_gap = *part;
    *part = SeriesRule SERIES_RULE_NONE;
    series_rule_polynomial_sum(one_h, one_h, factor);
    series_rule_part(factor, 1u, CORE_RULE_ANGULAR, 0, 0u, part);
    series_rule_polynomial_sum(a, a, factor);
    series_rule_part(factor, 1u, CORE_RULE_AXIAL, 0, 0u, other);
    const int printed = !series_rule_zero(&theta_gap) && !series_rule_zero(&along_gap) &&
                        core_rule_equal(&theta_gap, part, -1) && core_rule_equal(&along_gap, other, -1);
    series_rule_released(&theta_gap);
    series_rule_released(&along_gap);
    core_rule_report(&results, printed,
                     "  the paper's printed (17) and (18) differ from its (3) and (4) by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k",
                     "  the paper's printed (17) and (18) differ from its (3) and (4) otherwise", "the printed rule against the system");

    // 3. the parities carried one order
    static const int parities[6] = {1, -1, 1, 1, 1, 1};
    const int carried = (series_rule_parity(theta_right, parities) == 1) && (series_rule_parity(along_right, parities) == -1) &&
                        (series_rule_parity(flow_right, parities) == 1) && (series_rule_parity(rise_right, parities) == 1);
    core_rule_report(&results, carried, "  F, v0 and Pi even and U odd at every order to k give them at the next order",
                     "  the rule does not carry the parities one order", "the parities at every order");

    // 4. and 5.
    const int fits = !series_rule_short() && !run_cfg_short() && !record_short();
    core_rule_report(&results, fits, "  every exact value is stored in the build's width",
                     "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS",
                     "every exact value stored");
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        static const char *const s_written_names[8] = {
            "theta, the system's coefficient of X^k", "theta, the rule's left side less its right",
            "z, the system's coefficient of X^k",     "z, the rule's left side less its right",
            "v0, the system's coefficient of X^k",    "v0, the rule's left side less its right",
            "Pi, the system's coefficient of X^k",    "Pi, the rule's left side less its right"};
        const SeriesRule *const written[8] = {theta, theta_rule, along, along_rule, flow, flow_rule, rise, rise_rule};
        for (unsigned int each = 0u; each < 8u; each += 1u)
        {
            PlatformText line;
            record_line_begun(&line);
            mmgr_line_put(&line.line, "expression ");
            mmgr_line_put(&line.line, s_written_names[each]);
            mmgr_line_put(&line.line, ", ");
            mmgr_line_decimal(&line.line, series_rule_parts(written[each]), 1u);
            mmgr_line_put(&line.line, " parts");
            record_line_ended(record, &line);
            record_line_begun(&line);
            series_rule_text(written[each], names, &line.line);
            record_line_ended(record, &line);
        }
        recorded = record_close(record);
    }
    core_rule_report(&results, recorded, "  every expression is written whole to the cfg's record",
                     "  the record is not written: the cfg names none, or it does not open", "record written");
    for (unsigned int each = 0u; each < RULES; each += 1u)
    {
        series_rule_released(&s_rule[each]);
    }
    for (unsigned int each = 0u; each < POLYNOMIALS; each += 1u)
    {
        series_rule_polynomial_released(&s_polynomial[each]);
    }
    return sim_close(&results, "core rule");
}
