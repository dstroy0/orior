// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching_values.cu: Duraiswami's six matching functions valued, every term by its own exact series at each length
// the cfg names (matching.h, term_value.h)
#include "run_cfg.h"

#include "report.h"

#include "term_value.h"
#include "decay_integral.h"
#include "matching.h"
#include "ode_series.h"
#include "record.h"

// The six functions at each eta are the exact forms of matching_functions. At each length every term is given by the
// series term_value.h writes it in, summed to that length, and each form's value is an exact rational, written at each
// length and whole to the record.
// Checks:
// 1. Every term the forms hold is one term_value.h writes in its independent numbers.
// 2. At each z the cfg names, w and w' by their series about 1 and by their integral agree within the cfg's agreement
//    at the last length.
// 3. Every function's values at the last two lengths agree within the cfg's agreement.
// 4. Every exact value is held in the build's width.
// 5. Every value is written whole to the record the cfg names.
// The request: matching_values <cfg>.
//     SIM_EXACT_LIMBS=1024 bash examples/navier_stokes/run.sh matching_values examples/navier_stokes/cfg/matching_values.cfg

static const char *const s_matching_values_names[6] = {"torque", "force", "m_inf", "j_inf", "s_inf", "h_match"};

// 1 where |left - right| <= bound
static int matching_values_near(SimRational left, SimRational right, SimRational bound)
{
    return sim_rational_sign(sim_rational_difference(sim_rational_absolute(sim_rational_difference(left, right)), bound)) <= 0;
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
    std::vector<unsigned long long> lengths;
    std::vector<SimRational> kummer_checks;
    SimRational agreement;
    unsigned long long places = 0ull;
    int read = (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
               matching_read(&cfg, &request, &series) && run_cfg_rationals(&cfg, "etas", &etas) && !etas.empty() &&
               run_cfg_counts(&cfg, "lengths", &lengths) && (lengths.size() >= 2u) &&
               run_cfg_rationals(&cfg, "kummer_checks", &kummer_checks) && run_cfg_rational(&cfg, "agreement", &agreement) &&
               (sim_rational_sign(agreement) > 0) && run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull);
    for (size_t index = 0u; read && (index < etas.size()); index += 1u)
    {
        read = sim_rational_sign(sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(etas[index], etas[index]))) > 0;
    }
    for (size_t index = 0u; read && (index < lengths.size()); index += 1u)
    {
        read = (lengths[index] >= 2ull) && (lengths[index] <= 256ull);
    }
    for (size_t index = 0u; read && (index < kummer_checks.size()); index += 1u)
    {
        const SimRational offset = sim_rational_absolute(sim_rational_difference(kummer_checks[index], sim_rational(1ll, 1ll)));
        read = sim_rational_sign(sim_rational_difference(offset, sim_rational(1ll, 1ll))) < 0;
    }
    if (!read)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core anisotropy, axis swirl, axial and pressure, core order, join amplitude, inner "
                                           "and outer, content swirl and axial each as eta_modes and weights, blend cuts, terms "
                                           "above 2 and orders, etas inside (-1, 1), two or more lengths from 2 to 256, "
                                           "kummer_checks within 1 of 1, agreement above 0, report places");
        }
        sim_flush(&results);
        fprintf(stderr, "matching_values <cfg>\n");
        return 2;
    }
    const SimRational h = request.h;

    static TermBook book;
    std::vector<MatchingFunctions> functions(etas.size());
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        matching_at(&request, etas[index], &book, &functions[index]);
    }
    scriptura_text(&results.line, "  ");
    scriptura_decimal(&results.line, book.names.size(), 1u);
    scriptura_text(&results.line, " terms in the forms at ");
    scriptura_decimal(&results.line, etas.size(), 1u);
    scriptura_text(&results.line, " etas\n");
    sim_flush(&results);

    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int named = 1;
    int kummer = 1;
    // values[length][eta][function]
    std::vector<std::vector<std::vector<SimRational>>> values(lengths.size());
    static TermValues numbers;
    for (size_t step = 0u; step < lengths.size(); step += 1u)
    {
        term_value_open(&numbers, h, (unsigned int)lengths[step]);
        std::vector<SimRational> terms(book.names.size());
        for (size_t term = 0u; term < book.names.size(); term += 1u)
        {
            if (!term_value_named(&numbers, book.names[term], &terms[term]))
            {
                named = 0;
                terms[term] = sim_rational(0ll, 1ll);
                scriptura_text(&results.line, "  no value for the term ");
                scriptura_text(&results.line, book.names[term].c_str());
                scriptura_character(&results.line, '\n');
            }
        }
        scriptura_text(&results.line, "  length ");
        scriptura_decimal(&results.line, lengths[step], 1u);
        scriptura_text(&results.line, ": e ");
        report_value(&results.line, numbers.e, (unsigned int)places);
        scriptura_text(&results.line, ", Gamma(1 + h) ");
        report_value(&results.line, numbers.gamma_once, (unsigned int)places);
        scriptura_text(&results.line, ", w(1) ");
        report_value(&results.line, numbers.value_at_one, (unsigned int)places);
        scriptura_text(&results.line, ", w'(1) ");
        report_value(&results.line, numbers.slope_at_one, (unsigned int)places);
        scriptura_character(&results.line, '\n');
        if (record != NULL)
        {
            const std::string at = "length_" + std::to_string(lengths[step]);
            for (size_t term = 0u; term < book.names.size(); term += 1u)
            {
                record_form(record, (at + "_term_" + std::to_string(term)).c_str(), term_form_rational(terms[term]), &book);
            }
        }
        values[step].assign(etas.size(), std::vector<SimRational>(6u));
        for (size_t index = 0u; index < etas.size(); index += 1u)
        {
            const TermForm *const six[6] = {&functions[index].torque, &functions[index].force, &functions[index].m_inf,
                                            &functions[index].j_inf, &functions[index].s_inf, &functions[index].h_match};
            scriptura_text(&results.line, "    eta = ");
            report_value(&results.line, etas[index], (unsigned int)places);
            scriptura_character(&results.line, ':');
            for (size_t which = 0u; which < 6u; which += 1u)
            {
                const SimRational value = term_value_form(&numbers, *six[which], terms);
                values[step][index][which] = value;
                scriptura_character(&results.line, ' ');
                scriptura_text(&results.line, s_matching_values_names[which]);
                scriptura_character(&results.line, ' ');
                report_value(&results.line, value, (unsigned int)places);
                if (record != NULL)
                {
                    const std::string at = "length_" + std::to_string(lengths[step]) + "_eta_" + term_book_rational(etas[index]) +
                                           "_" + s_matching_values_names[which];
                    record_form(record, at.c_str(), term_form_rational(value), &book);
                }
            }
            scriptura_character(&results.line, '\n');
        }
        sim_flush(&results);
        // 2. the two routes to w at the last length
        if (step + 1u == lengths.size())
        {
            for (const SimRational &z : kummer_checks)
            {
                SimRational series_value;
                SimRational series_slope;
                SimRational integral_value;
                SimRational integral_slope;
                term_value_kummer(&numbers, z, &series_value, &series_slope);
                term_value_integral(&numbers, z, &integral_value, &integral_slope);
                const int near = matching_values_near(series_value, integral_value, agreement) &&
                                 matching_values_near(series_slope, integral_slope, agreement);
                kummer = kummer && near;
                scriptura_text(&results.line, "    z = ");
                report_value(&results.line, z, (unsigned int)places);
                scriptura_text(&results.line, ": w by its series ");
                report_value(&results.line, series_value, (unsigned int)places);
                scriptura_text(&results.line, ", by its integral ");
                report_value(&results.line, integral_value, (unsigned int)places);
                scriptura_text(&results.line, "; w' ");
                report_value(&results.line, series_slope, (unsigned int)places);
                scriptura_text(&results.line, ", ");
                report_value(&results.line, integral_slope, (unsigned int)places);
                scriptura_character(&results.line, '\n');
            }
        }
    }

    // 1. the terms
    scriptura_text(&results.line, named ? "  every term is written in its independent numbers\n"
                                        : "  an term has no value\n");
    sim_check(&results, named, "every term valued");
    // 2. the two routes to w
    scriptura_text(&results.line, kummer ? "  w and w' by their series about 1 and by their integral agree within the cfg's agreement\n"
                                         : "  w or w' by its series about 1 and by its integral differ past the cfg's agreement\n");
    sim_check(&results, kummer, "two routes to w");
    // 3. the last two lengths
    // each change is set against the agreement alone: two changes set against each other pass the width
    int settled = 1;
    const size_t last = lengths.size() - 1u;
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        scriptura_text(&results.line, "  the change between the last two lengths at eta = ");
        report_value(&results.line, etas[index], (unsigned int)places);
        scriptura_character(&results.line, ':');
        for (size_t which = 0u; which < 6u; which += 1u)
        {
            const SimRational change =
                sim_rational_absolute(sim_rational_difference(values[last][index][which], values[last - 1u][index][which]));
            settled = settled && (sim_rational_sign(sim_rational_difference(change, agreement)) <= 0);
            scriptura_character(&results.line, ' ');
            scriptura_text(&results.line, s_matching_values_names[which]);
            scriptura_character(&results.line, ' ');
            report_value(&results.line, change, 2u);
        }
        scriptura_character(&results.line, '\n');
    }
    sim_check(&results, settled, "the last two lengths agree");
    // 5. the record, before the width
    const int recorded = (record != NULL) && record_close(record);
    // 4. the width
    const int wide[14] = {s_sim_rational_wide,    run_cfg_short(),    report_short(),      term_form_short(),
                          taylor_short(),         ode_series_short(), eta_function_short(), core_series_short(),
                          decay_integral_short(), blend_short(),      blend_field_short(),  matching_short(),
                          term_value_short(),     record_short()};
    static const char *const modules[14] = {"matching_values", "run_cfg", "report", "term_form", "taylor", "ode_series",
                                            "eta_function", "core_series", "decay_integral", "blend", "blend_field",
                                            "matching", "term_value", "record"};
    int held = 1;
    for (size_t module = 0u; module < 14u; module += 1u)
    {
        if (wide[module] != 0)
        {
            held = 0;
            scriptura_text(&results.line, "  a value outgrew the build's width in ");
            scriptura_text(&results.line, modules[module]);
            scriptura_character(&results.line, '\n');
        }
    }
    if (held)
    {
        scriptura_text(&results.line, "  every exact value is held in the build's width\n");
    }
    sim_check(&results, held, "every exact value held");
    scriptura_text(&results.line, recorded ? "  every value is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "matching values");
}
