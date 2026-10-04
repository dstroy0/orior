// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching_functions.cu: Duraiswami's six matching functions at each eta the cfg names, exact forms (matching.h)
#include "run_cfg.h"

#include "report.h"

#include "decay_integral.h"
#include "matching.h"
#include "ode_series.h"
#include "record.h"

// The axis data, the amplitude c, the join and the annulus content are the cfg's. At each eta the six functions are
// exact forms in held terms, written whole to the record. c^2 X_b^(-2h) / (4h), the power part of the tail of S, is
// X_b to a rational power: with 2h = r / q, y lies below it exactly where (y 4h / c^2)^q X_b^r < 1, and the cfg's
// decimal for it, with its places, is checked by two such comparisons.
// Checks:
// 1. E_m for m = 0 down to -2 satisfies (k + 1) r - x r' + x r = 1 exactly at each x the cfg names.
// 2. With the blend switched off the core alone fills the annulus, and its torque and force, the core's residuals
//    integrated with every boundary term, are no more than the cfg's residual at the first eta.
// 3. The fields' values at X_a and X_b are there to take at every eta.
// 4. The power part of the tail of S lies within half a unit of the cfg's decimal's last place of that decimal.
// 5. Every exact value is held in the build's width, and no end integral diverges.
// 6. Every form is written whole to the record the cfg names.
// The request: matching_functions <cfg>.
//     bash examples/navier_stokes/run.sh matching_functions examples/navier_stokes/cfg/matching_functions.cfg

// base^power for a whole power
static SimRational matching_functions_power(SimRational base, unsigned long long power)
{
    SimRational value = sim_rational(1ll, 1ll);
    for (unsigned long long step = 0ull; step < power; step += 1ull)
    {
        value = sim_rational_product(value, base);
    }
    return value;
}

// 1 where y is below c^2 X_b^(-2h) / (4h): (y 4h / c^2)^q X_b^r < 1, 2h = r / q
static int matching_functions_below(SimRational y, SimRational h, SimRational c, SimRational reach)
{
    const SimRational twice_h = sim_rational_product(sim_rational(2ll, 1ll), h);
    long long r = 0ll;
    long long q = 0ll;
    if (!sim_rational_small(&twice_h.numerator, &r) || !sim_rational_small(&twice_h.denominator, &q) || (r <= 0ll) ||
        (q <= 0ll) || (q > 4096ll) || (r > 4096ll))
    {
        return -1;
    }
    const SimRational scaled = sim_rational_product(sim_rational_product(y, sim_rational_product(sim_rational(4ll, 1ll), h)),
                                                    sim_rational_reciprocal(sim_rational_product(c, c)));
    const SimRational left = sim_rational_product(matching_functions_power(scaled, (unsigned long long)q),
                                                  matching_functions_power(reach, (unsigned long long)r));
    return sim_rational_sign(sim_rational_difference(left, sim_rational(1ll, 1ll))) < 0;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    static MatchingRequest request;
    static CoreSeries series;
    std::vector<SimRational> etas;
    std::vector<SimRational> checks;
    SimRational printed;
    SimRational unit;
    SimRational residual;
    unsigned long long places = 0ull;
    int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) && matching_read(&cfg, &request, &series) &&
        run_cfg_rationals(&cfg, "etas", &etas) && run_cfg_rationals(&cfg, "decay_checks", &checks) &&
        run_cfg_rational(&cfg, "his.s_tail", &printed) && run_cfg_rational(&cfg, "his.s_tail_unit", &unit) &&
        (sim_rational_sign(unit) > 0) && run_cfg_rational(&cfg, "core_alone.residual", &residual) &&
        (sim_rational_sign(residual) > 0) && run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull) &&
        !etas.empty();
    for (size_t index = 0u; read && (index < etas.size()); index += 1u)
    {
        const SimRational rest =
            sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(etas[index], etas[index]));
        read = sim_rational_sign(rest) > 0;
    }
    for (size_t index = 0u; read && (index < checks.size()); index += 1u)
    {
        read = sim_rational_sign(checks[index]) > 0;
    }
    if (!read)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core anisotropy, axis swirl, axial and pressure, core order, join "
                                           "amplitude, inner and outer, content swirl and axial each as eta_modes and "
                                           "weights, blend cuts, terms above 2 and orders, etas inside (-1, 1), "
                                           "decay_checks above 0, his s_tail and s_tail_unit, core_alone residual above "
                                           "0, report places");
        }
        sim_flush(&results);
        fprintf(stderr, "matching_functions <cfg>\n");
        return 2;
    }
    const SimRational h = request.h;

    static TermBook book;
    // the core alone on the annulus, at the first eta
    request.blended = 0;
    static MatchingFunctions alone;
    matching_at(&request, etas[0], &book, &alone);
    request.blended = 1;
    const SimRational torque_alone = term_form_largest(alone.torque);
    const SimRational force_alone = term_form_largest(alone.force);
    scriptura_text(&results.line, "  the core alone at eta = ");
    report_value(&results.line, etas[0], (unsigned int)places);
    scriptura_text(&results.line, ": torque ");
    report_value(&results.line, torque_alone, (unsigned int)places);
    scriptura_text(&results.line, ", force ");
    report_value(&results.line, force_alone, (unsigned int)places);
    scriptura_text(&results.line, " (largest coefficient), M ");
    report_value(&results.line, term_form_constant(alone.m_inf), (unsigned int)places);
    scriptura_character(&results.line, '\n');
    std::vector<MatchingFunctions> functions(etas.size());
    int ends = alone.ends;
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        matching_at(&request, etas[index], &book, &functions[index]);
        ends = ends && functions[index].ends;
        scriptura_text(&results.line, "  eta = ");
        report_value(&results.line, etas[index], (unsigned int)places);
        scriptura_text(&results.line, ": terms in torque, force, M, J, S, H:");
        const TermForm *const six[6] = {&functions[index].torque, &functions[index].force, &functions[index].m_inf,
                                        &functions[index].j_inf, &functions[index].s_inf, &functions[index].h_match};
        for (const TermForm *form : six)
        {
            scriptura_character(&results.line, ' ');
            scriptura_decimal(&results.line, term_form_terms(*form), 1u);
        }
        scriptura_text(&results.line, "\n    the part of M with no term: ");
        report_value(&results.line, term_form_constant(functions[index].m_inf), (unsigned int)places);
        scriptura_character(&results.line, '\n');
        sim_flush(&results);
    }
    scriptura_text(&results.line, "  ");
    scriptura_decimal(&results.line, book.names.size(), 1u);
    scriptura_text(&results.line, " terms\n");

    // 1. the decay integral below k = -1
    int decay = 1;
    for (const SimRational &x : checks)
    {
        for (int k = -2; k >= -4; k -= 1)
        {
            decay = decay && (sim_rational_sign(decay_integral_negative_residual(k, x)) == 0);
        }
    }
    scriptura_text(&results.line, decay ? "  E_0, E_-1, E_-2: (k + 1) r - x r' + x r = 1 exactly at every x\n"
                                        : "  a negative-order E_m does not satisfy its derivative identity\n");
    sim_check(&results, decay, "decay integral below k = -1");
    // 2. the core alone
    const int small = (sim_rational_sign(sim_rational_difference(torque_alone, residual)) <= 0) &&
                      (sim_rational_sign(sim_rational_difference(force_alone, residual)) <= 0);
    scriptura_text(&results.line, small ? "  the core alone: torque and force within the cfg's residual\n"
                                        : "  the core alone: torque or force past the cfg's residual\n");
    sim_check(&results, small, "the core alone");
    // 3. the ends
    scriptura_text(&results.line, ends ? "  the values at X_a and X_b are there at every eta\n"
                                       : "  a value at X_a or X_b is singular\n");
    sim_check(&results, ends, "values at the ends");
    // 4. the power part of the tail of S against his decimal
    const SimRational half_unit = sim_rational_product(unit, sim_rational(1ll, 2ll));
    const int low_below = matching_functions_below(sim_rational_difference(printed, half_unit), h, request.amplitude, request.outer);
    const int high_below = matching_functions_below(sim_rational_sum(printed, half_unit), h, request.amplitude, request.outer);
    const int within = (low_below == 1) && (high_below == 0);
    scriptura_text(&results.line, "  c^2 X_b^(-2h) / (4h) = ");
    term_form_print(&results.line, functions.empty() ? TermForm() : functions[0].s_tail_power, book.names, (unsigned int)places);
    scriptura_text(&results.line, within ? ", within half a unit of the last place of his "
                                         : ", not within half a unit of the last place of his ");
    report_value(&results.line, printed, (unsigned int)places);
    scriptura_character(&results.line, '\n');
    sim_check(&results, within, "the tail of S against his decimal");
    // 6. the record, before the width
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        for (size_t index = 0u; index < etas.size(); index += 1u)
        {
            const std::string at = "eta_" + term_book_rational(etas[index]);
            record_form(record, (at + "_torque").c_str(), functions[index].torque, &book);
            record_form(record, (at + "_force").c_str(), functions[index].force, &book);
            record_form(record, (at + "_m_inf").c_str(), functions[index].m_inf, &book);
            record_form(record, (at + "_j_inf").c_str(), functions[index].j_inf, &book);
            record_form(record, (at + "_s_inf").c_str(), functions[index].s_inf, &book);
            record_form(record, (at + "_h_match").c_str(), functions[index].h_match, &book);
        }
        recorded = record_close(record);
    }
    // 5. the width
    const int held = (s_sim_rational_wide == 0) && (run_cfg_short() == 0) && (report_short() == 0) &&
                     (term_form_short() == 0) && (taylor_short() == 0) && (ode_series_short() == 0) &&
                     (eta_function_short() == 0) && (core_series_short() == 0) && (decay_integral_short() == 0) &&
                     (blend_short() == 0) && (blend_field_short() == 0) && (matching_short() == 0) &&
                     (record_short() == 0);
    scriptura_text(&results.line, held ? "  every exact value is held in the build's width, and no end integral diverges\n"
                                       : "  a value outgrew the build's width, or an end integral diverges\n");
    sim_check(&results, held, "every exact value held");
    scriptura_text(&results.line, recorded ? "  every form is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "matching functions");
}
