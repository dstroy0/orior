// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// join_datum.cu: the right side of Duraiswami's pressure datum at one eta, an exact form in terms (pressure_datum.h)
#include "run_cfg.h"

#include "report.h"

#include "decay_integral.h"
#include "ode_series.h"
#include "pressure_datum.h"
#include "record.h"

// The value returned is the right side for the cfg's axis data and datum Pi_0, every term held.
// Checks:
// 1. Every exact value is held in the build's width.
// 2. The two-term decay integral's derivative is b^k e^(-n/b) exactly, for every k the pieces use.
// 3. Every form is written whole to the record the cfg names.
// The request: join_datum <cfg>.
//     bash examples/navier_stokes/run.sh join_datum examples/navier_stokes/cfg/join_datum.cfg

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    SimRational eta;
    SimRational inner;
    SimRational outer;
    std::vector<SimRational> swirl;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    static PressureDatumRequest request;
    unsigned long long order = 0ull;
    unsigned long long terms = 0ull;
    unsigned long long orders = 0ull;
    unsigned long long places = 0ull;
    const int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
        run_cfg_rational(&cfg, "core.anisotropy", &h) && (sim_rational_sign(h) > 0) &&
        run_cfg_rationals(&cfg, "core.axis.swirl", &swirl) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
        run_cfg_rationals(&cfg, "core.axis.pressure", &pressure) && run_cfg_count(&cfg, "core.order", &order) &&
        (order > 0ull) && (order < (1ull << 20u)) && run_cfg_rational(&cfg, "join.eta", &eta) &&
        run_cfg_rational(&cfg, "join.amplitude", &request.amplitude) && run_cfg_rational(&cfg, "join.inner", &inner) &&
        run_cfg_rational(&cfg, "join.outer", &outer) &&
        (sim_rational_sign(sim_rational_difference(outer, inner)) > 0) && (sim_rational_sign(inner) > 0) &&
        run_cfg_rationals(&cfg, "blend.cuts", &request.pieces.cuts) && (request.pieces.cuts.size() >= 3u) &&
        run_cfg_count(&cfg, "blend.terms", &terms) && (terms > 1ull) && run_cfg_count(&cfg, "blend.orders", &orders) &&
        run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull) &&
        (sim_rational_sign(sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(eta, eta))) > 0);
    if (!read)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core anisotropy, axis swirl, axial and pressure, core order, join eta "
                                           "inside (-1, 1), amplitude, inner and outer with 0 < inner < outer, blend "
                                           "cuts (three or more), blend terms and orders, report places");
        }
        sim_flush(&results);
        fprintf(stderr, "join_datum <cfg>\n");
        return 2;
    }
    request.pieces.terms = (unsigned int)terms;
    request.pieces.orders = (unsigned int)orders;
    request.h = h;
    request.shape.part = h.numerator;
    request.shape.whole = h.denominator;
    EtaFunction swirl_data;
    EtaFunction axial_data;
    EtaFunction pressure_data;
    eta_function_chebyshev(swirl, &swirl_data);
    eta_function_chebyshev(axial, &axial_data);
    eta_function_chebyshev(pressure, &pressure_data);
    static CoreSeries series;
    core_series_recursion(&request.shape, &swirl_data, &axial_data, &pressure_data, (unsigned int)order, &series);
    request.series = &series;
    static TermBook book;
    static PressureDatum datum;
    pressure_datum_at(&request, eta, inner, outer, &book, &datum);

    scriptura_text(&results.line, "  join datum at eta = ");
    report_value(&results.line, eta, (unsigned int)places);
    scriptura_text(&results.line, ": ");
    scriptura_decimal(&results.line, term_form_terms(datum.datum), 1u);
    scriptura_text(&results.line, " terms over ");
    scriptura_decimal(&results.line, book.names.size(), 1u);
    scriptura_text(&results.line, " terms and ");
    scriptura_decimal(&results.line, term_form_e_count(datum.datum), 1u);
    scriptura_text(&results.line, " powers of e, held as ");
    scriptura_decimal(&results.line, term_form_entries(datum.datum), 1u);
    scriptura_text(&results.line, " entries of at most ");
    scriptura_decimal(&results.line, term_form_bits(datum.datum), 1u);
    scriptura_text(&results.line, " bits over one denominator\n  the core inside X_a, -int_0^{X_a} F_core^2 dX = ");
    report_value(&results.line, sim_rational_negative(term_form_constant(datum.inside)), (unsigned int)places);
    scriptura_text(&results.line, "\n  the terms:");
    for (const std::string &name : book.names)
    {
        scriptura_text(&results.line, " ");
        scriptura_text(&results.line, name.c_str());
    }
    scriptura_character(&results.line, '\n');
    // the record: every form whole
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        record_form(record, "core_inside", datum.inside, &book);
        record_form(record, "annulus", datum.annulus, &book);
        record_form(record, "beyond", datum.beyond, &book);
        record_form(record, "datum", datum.datum, &book);
        recorded = record_close(record);
    }
    // 2. the decay integral, every k the pieces use
    int derivative = 1;
    for (unsigned int k = 0u; k < request.pieces.terms; k += 1u)
    {
        std::vector<SimRational> first;
        std::vector<SimRational> second;
        decay_integral_residual(k, &first, &second);
        derivative = derivative && first.empty() && second.empty();
    }
    // 1. the width
    const int held = (s_sim_rational_wide == 0) && (run_cfg_short() == 0) && (report_short() == 0) &&
                     (term_form_short() == 0) && (taylor_short() == 0) && (ode_series_short() == 0) &&
                     (eta_function_short() == 0) && (core_series_short() == 0) && (decay_integral_short() == 0) &&
                     (blend_short() == 0) && (pressure_datum_short() == 0) && (record_short() == 0);
    scriptura_text(&results.line, held ? "  every exact value is held in the build's width\n"
                                       : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every exact value held");
    scriptura_text(&results.line, derivative ? "  d/db of each decay integral is b^k e^(-n/b) exactly\n"
                                             : "  a decay integral's derivative is not b^k e^(-n/b)\n");
    sim_check(&results, derivative, "decay integral derivative");
    scriptura_text(&results.line, recorded ? "  every form is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "join datum");
}
