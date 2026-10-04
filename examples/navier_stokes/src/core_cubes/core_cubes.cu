// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_cubes.cu: the axis core measured by witness cubes over (X, eta, h), at each series order the cfg names
#include "run_cfg.h"

#include "report.h"

#include "core_series.h"
#include "record.h"
#include "witness_cube.h"

// The subject at (X, eta, h) is F [F] + U [U] + v0 [v0] + Pi [Pi], each field the core series cut after X^K and summed
// exactly at X, and [F], [U], [v0], [Pi] four atoms that name the field a term belongs to. The series is computed once
// for each of the two values of h the cubes take, to the highest order asked: the coefficients of order K do not
// depend on where the series is cut, and a lower order is the same series cut sooner. For each cube and each pair of
// consecutive orders the difference of the components is recorded.
// The axis data are the cfg's Chebyshev weights in eta, the pressure among them.
// Checks:
// 1. Every cube's components rebuild its 8 corners exactly.
// 2. Every exact value is held in the build's width.
// 3. Every cube is written whole to the record the cfg names.
// The request: core_cubes <cfg>.
//     SIM_EXACT_LIMBS=512 bash examples/navier_stokes/run.sh core_cubes examples/navier_stokes/cfg/core_cubes.cfg

#define CORE_CUBES_FIELDS 4u

static const char *const s_core_cubes_fields[CORE_CUBES_FIELDS] = {"F", "U", "v0", "Pi"};

typedef struct
{
    SimRational h[2];
    EtaShape shape[2];
    CoreSeries series[2];
    unsigned int cut;
    unsigned int atom[CORE_CUBES_FIELDS];
} CoreCubes;

static const std::vector<EtaFunction> &core_cubes_field(const CoreSeries *series, unsigned int field)
{
    if (field == 0u)
    {
        return series->swirl;
    }
    if (field == 1u)
    {
        return series->axial;
    }
    return (field == 2u) ? series->inflow : series->pressure;
}

static AtomForm core_cubes_subject(const void *context, const SimRational *point, AtomBook *book)
{
    const CoreCubes *const cubes = (const CoreCubes *)context;
    (void)book;
    const unsigned int side = sim_rational_equal(point[2], cubes->h[0]) ? 0u : 1u;
    AtomForm value;
    for (unsigned int field = 0u; field < CORE_CUBES_FIELDS; field += 1u)
    {
        const std::vector<EtaFunction> &whole = core_cubes_field(&cubes->series[side], field);
        const size_t terms = (whole.size() < (size_t)cubes->cut + 1u) ? whole.size() : (size_t)cubes->cut + 1u;
        const std::vector<EtaFunction> cut(whole.begin(), whole.begin() + (long)terms);
        const std::vector<SimRational> coefficients = core_series_at(&cubes->shape[side], cut, point[1]);
        SimRational sum = sim_rational(0ll, 1ll);
        for (size_t index = coefficients.size(); index > 0u; index -= 1u)
        {
            sum = sim_rational_sum(sim_rational_product(sum, point[0]), coefficients[index - 1u]);
        }
        value = atom_form_sum(value, atom_form_scaled(atom_form_atom(cubes->atom[field]), sum));
    }
    return value;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    static CoreCubes cubes;
    std::vector<SimRational> swirl;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    std::vector<SimRational> centers;
    std::vector<SimRational> halves;
    std::vector<unsigned long long> orders_read;
    unsigned long long places = 0ull;
    const int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
        run_cfg_rationals(&cfg, "core.axis.swirl", &swirl) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
        run_cfg_rationals(&cfg, "core.axis.pressure", &pressure) && run_cfg_rationals(&cfg, "cubes.centers", &centers) &&
        (centers.size() >= 3u) && ((centers.size() % 3u) == 0u) && run_cfg_rationals(&cfg, "cubes.half", &halves) &&
        (halves.size() == 3u) && run_cfg_counts(&cfg, "core.orders", &orders_read) &&
        run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull);
    std::vector<unsigned int> orders;
    int usable = read;
    for (size_t index = 0u; usable && (index < orders_read.size()); index += 1u)
    {
        usable = (orders_read[index] > 0ull) && (orders_read[index] < 4096ull) &&
                 (orders.empty() || ((unsigned int)orders_read[index] > orders.back()));
        // below 4096, checked above
        orders.push_back((unsigned int)orders_read[index]);
    }
    for (unsigned int axis = 0u; usable && (axis < 3u); axis += 1u)
    {
        usable = sim_rational_sign(halves[axis]) > 0;
    }
    // every cube shares the h axis: its center and half-edge are the same in each
    for (size_t index = 3u; usable && (index < centers.size()); index += 3u)
    {
        usable = sim_rational_equal(centers[index + 2u], centers[2]);
    }
    if (!usable)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core axis swirl, axial and pressure, core orders as whole numbers rising "
                                           "below 4096, cubes centers as (X, eta, h) triples sharing h, cubes half as "
                                           "three half-edges above 0, report places");
        }
        sim_flush(&results);
        fprintf(stderr, "core_cubes <cfg>\n");
        return 2;
    }
    EtaFunction swirl_data;
    EtaFunction axial_data;
    EtaFunction pressure_data;
    eta_function_chebyshev(swirl, &swirl_data);
    eta_function_chebyshev(axial, &axial_data);
    eta_function_chebyshev(pressure, &pressure_data);
    cubes.h[0] = sim_rational_difference(centers[2], halves[2]);
    cubes.h[1] = sim_rational_sum(centers[2], halves[2]);
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        cubes.shape[side].part = cubes.h[side].numerator;
        cubes.shape[side].whole = cubes.h[side].denominator;
        core_series_recursion(&cubes.shape[side], &swirl_data, &axial_data, &pressure_data, orders.back(),
                              &cubes.series[side]);
    }
    static AtomBook book;
    for (unsigned int field = 0u; field < CORE_CUBES_FIELDS; field += 1u)
    {
        cubes.atom[field] = atom_book_id(&book, s_core_cubes_fields[field]);
    }

    const size_t cube_count = centers.size() / 3u;
    std::vector<std::vector<WitnessCube>> measured(cube_count, std::vector<WitnessCube>(orders.size()));
    int whole = 1;
    for (size_t cube = 0u; cube < cube_count; cube += 1u)
    {
        for (size_t order = 0u; order < orders.size(); order += 1u)
        {
            cubes.cut = orders[order];
            witness_cube_measure(core_cubes_subject, &cubes, &centers[3u * cube], halves.data(), &book,
                                 &measured[cube][order]);
            whole = whole && witness_cube_whole(&measured[cube][order]);
        }
    }
    for (size_t cube = 0u; cube < cube_count; cube += 1u)
    {
        scriptura_text(&results.line, "  cube about (X, eta, h) = (");
        for (unsigned int axis = 0u; axis < 3u; axis += 1u)
        {
            report_value(&results.line, centers[3u * cube + axis], (unsigned int)places);
            scriptura_text(&results.line, (axis < 2u) ? ", " : ")\n");
        }
        for (size_t order = 0u; order < orders.size(); order += 1u)
        {
            const WitnessCube *const now = &measured[cube][order];
            scriptura_text(&results.line, "    order ");
            scriptura_decimal(&results.line, orders[order], 1u);
            scriptura_text(&results.line, ", characters 1..8:");
            for (unsigned int character = 1u; character <= 8u; character += 1u)
            {
                scriptura_character(&results.line, ' ');
                scriptura_decimal(&results.line, witness_cube_characters(now, character), 1u);
            }
            scriptura_character(&results.line, '\n');
            for (unsigned int field = 0u; field < CORE_CUBES_FIELDS; field += 1u)
            {
                scriptura_text(&results.line, "      ");
                scriptura_text(&results.line, s_core_cubes_fields[field]);
                scriptura_text(&results.line, ":");
                for (unsigned int component = 0u; component < 8u; component += 1u)
                {
                    scriptura_character(&results.line, ' ');
                    report_value(&results.line, atom_form_coefficient_of(now->component[component], cubes.atom[field]),
                                 (unsigned int)places);
                }
                scriptura_character(&results.line, '\n');
                if (order > 0u)
                {
                    const WitnessCube *const before = &measured[cube][order - 1u];
                    scriptura_text(&results.line, "      ");
                    scriptura_text(&results.line, s_core_cubes_fields[field]);
                    scriptura_text(&results.line, " less order ");
                    scriptura_decimal(&results.line, orders[order - 1u], 1u);
                    scriptura_text(&results.line, ":");
                    for (unsigned int component = 0u; component < 8u; component += 1u)
                    {
                        scriptura_character(&results.line, ' ');
                        report_value(&results.line,
                                     sim_rational_difference(
                                         atom_form_coefficient_of(now->component[component], cubes.atom[field]),
                                         atom_form_coefficient_of(before->component[component], cubes.atom[field])),
                                     (unsigned int)places);
                    }
                    scriptura_character(&results.line, '\n');
                }
            }
            sim_flush(&results);
        }
    }
    // 1. the corners rebuilt
    scriptura_text(&results.line, whole ? "  every cube's components rebuild its 8 corners exactly\n"
                                        : "  a cube's components do not rebuild a corner\n");
    sim_check(&results, whole, "corners rebuilt");
    // 3. the record, before the width: a number too wide to write marks the width
    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    int recorded = 0;
    if (record != NULL)
    {
        for (size_t cube = 0u; cube < cube_count; cube += 1u)
        {
            for (size_t order = 0u; order < orders.size(); order += 1u)
            {
                const std::string name = "cube_" + std::to_string(cube) + "_order_" + std::to_string(orders[order]);
                witness_cube_record(record, name.c_str(), &measured[cube][order], &book);
            }
        }
        recorded = record_close(record);
    }
    // 2. the width
    const int held = (s_sim_rational_wide == 0) && (run_cfg_short() == 0) && (report_short() == 0) &&
                     (atom_form_short() == 0) && (eta_function_short() == 0) && (core_series_short() == 0) &&
                     (record_short() == 0) && (witness_cube_short() == 0);
    scriptura_text(&results.line, held ? "  every exact value is held in the build's width\n"
                                       : "  a value outgrew the build's width: run with a larger SIM_EXACT_LIMBS\n");
    sim_check(&results, held, "every exact value held");
    scriptura_text(&results.line, recorded ? "  every cube is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "core cubes");
}
