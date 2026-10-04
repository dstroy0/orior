// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// datum_cubes.cu: the right side of the pressure datum measured by a witness cube over (X_a, X_b, eta)
#include "run_cfg.h"

#include "report.h"

#include "decay_integral.h"
#include "ode_series.h"
#include "pressure_datum.h"
#include "record.h"
#include "witness_cube.h"

// The subject at (X_a, X_b, eta) is the right side of the datum (pressure_datum.h) for the cfg's core, joined from X_a
// to X_b. What the answer should depend on is eta; the X_a and X_b components measure how much the join's own
// placement puts into it. The terms of the exterior sit at points that move with the corner, and the part with no
// term is reported by component.
// Checks:
// 1. The cube's components rebuild its 8 corners exactly.
// 2. Every exact value is held in the build's width.
// 3. The cube is written whole to the record the cfg names.
// The request: datum_cubes <cfg>.
//     bash examples/navier_stokes/run.sh datum_cubes examples/navier_stokes/cfg/datum_cubes.cfg

static TermForm datum_cubes_subject(const void *context, const SimRational *point, TermBook *book)
{
    const PressureDatumRequest *const request = (const PressureDatumRequest *)context;
    PressureDatum datum;
    pressure_datum_at(request, point[2], point[0], point[1], book, &datum);
    return datum.datum;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    SimRational h;
    std::vector<SimRational> swirl;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    std::vector<SimRational> center;
    std::vector<SimRational> half;
    static PressureDatumRequest request;
    unsigned long long order = 0ull;
    unsigned long long terms = 0ull;
    unsigned long long orders = 0ull;
    unsigned long long places = 0ull;
    int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
        run_cfg_rational(&cfg, "core.anisotropy", &h) && (sim_rational_sign(h) > 0) &&
        run_cfg_rationals(&cfg, "core.axis.swirl", &swirl) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
        run_cfg_rationals(&cfg, "core.axis.pressure", &pressure) && run_cfg_count(&cfg, "core.order", &order) &&
        (order > 0ull) && (order < (1ull << 20u)) && run_cfg_rational(&cfg, "join.amplitude", &request.amplitude) &&
        run_cfg_rationals(&cfg, "cube.center", &center) && (center.size() == 3u) &&
        run_cfg_rationals(&cfg, "cube.half", &half) && (half.size() == 3u) &&
        run_cfg_rationals(&cfg, "blend.cuts", &request.pieces.cuts) && (request.pieces.cuts.size() >= 3u) &&
        run_cfg_count(&cfg, "blend.terms", &terms) && (terms > 1ull) && run_cfg_count(&cfg, "blend.orders", &orders) &&
        run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull);
    if (read)
    {
        // 0 < X_a low, X_a high < X_b low, and eta's corners inside (-1, 1)
        const SimRational one = sim_rational(1ll, 1ll);
        read = (sim_rational_sign(half[0]) > 0) && (sim_rational_sign(half[1]) > 0) && (sim_rational_sign(half[2]) > 0) &&
               (sim_rational_sign(sim_rational_difference(center[0], half[0])) > 0) &&
               (sim_rational_sign(sim_rational_difference(sim_rational_difference(center[1], half[1]),
                                                          sim_rational_sum(center[0], half[0]))) > 0) &&
               (sim_rational_sign(sim_rational_difference(one, sim_rational_sum(center[2], half[2]))) > 0) &&
               (sim_rational_sign(sim_rational_sum(one, sim_rational_difference(center[2], half[2]))) > 0);
    }
    if (!read)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core anisotropy, axis swirl, axial and pressure, core order, join "
                                           "amplitude, cube center and half as (X_a, X_b, eta) with 0 < X_a, X_a < X_b "
                                           "and eta inside (-1, 1) at every corner, blend cuts (three or more), blend "
                                           "terms and orders, report places");
        }
        sim_flush(&results);
        fprintf(stderr, "datum_cubes <cfg>\n");
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
    static WitnessCube cube;
    witness_cube_measure(datum_cubes_subject, &request, center.data(), half.data(), &book, &cube);

    scriptura_text(&results.line, "  datum cube about (X_a, X_b, eta) = (");
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        report_value(&results.line, center[axis], (unsigned int)places);
        scriptura_text(&results.line, (axis < 2u) ? ", " : ")\n");
    }
    scriptura_text(&results.line, "    ");
    scriptura_decimal(&results.line, cube.slot.size(), 1u);
    scriptura_text(&results.line, " terms over ");
    scriptura_decimal(&results.line, book.names.size(), 1u);
    scriptura_text(&results.line, " terms; by character 1..8:");
    for (unsigned int character = 1u; character <= 8u; character += 1u)
    {
        scriptura_character(&results.line, ' ');
        scriptura_decimal(&results.line, witness_cube_characters(&cube, character), 1u);
    }
    scriptura_text(&results.line, "\n    the part with no term, components c0..c7:");
    for (unsigned int component = 0u; component < 8u; component += 1u)
    {
        scriptura_character(&results.line, ' ');
        report_value(&results.line, term_form_constant(cube.component[component]), (unsigned int)places);
    }
    scriptura_character(&results.line, '\n');
    // 1. the corners rebuilt
    const int whole = witness_cube_whole(&cube);
    scriptura_text(&results.line, whole ? "  the cube's components rebuild its 8 corners exactly\n"
                                        : "  the cube's components do not rebuild a corner\n");
    sim_check(&results, whole, "corners rebuilt");
    // 3. the record, before the width: a number too wide to write marks the width
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        witness_cube_record(record, "datum", &cube, &book);
        recorded = record_close(record);
    }
    // 2. the width
    const int held = (s_sim_rational_wide == 0) && (run_cfg_short() == 0) && (report_short() == 0) &&
                     (term_form_short() == 0) && (taylor_short() == 0) && (ode_series_short() == 0) &&
                     (eta_function_short() == 0) && (core_series_short() == 0) && (decay_integral_short() == 0) &&
                     (blend_short() == 0) && (pressure_datum_short() == 0) && (record_short() == 0) &&
                     (witness_cube_short() == 0);
    scriptura_text(&results.line, held ? "  every exact value is held in the build's width\n"
                                       : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every exact value held");
    scriptura_text(&results.line, recorded ? "  the cube is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "datum cubes");
}
