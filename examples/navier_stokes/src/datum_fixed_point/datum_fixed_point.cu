// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// datum_fixed_point.cu: the axis pressure Pi_0 that Duraiswami's datum returns, by Newton steps on decimal iterates
// (pressure_datum.h, atom_value.h)
#include "run_cfg.h"

#include "report.h"

#include "atom_value.h"
#include "decay_integral.h"
#include "ode_series.h"
#include "pressure_datum.h"
#include "record.h"

// Pi_0 = sum_(k<n) p_k T_k(eta), and G(p)(eta) the right side of the datum with the core built on that Pi_0, every atom
// valued by atom_value.h at the cfg's length. Pi_0 is a fixed point where Y_i = G(p)(eta_i) - p(eta_i) is 0 at each of
// the cfg's n nodes. A full exact iterate would carry G's values, thousands of digits each, into the next core. Each
// iterate is written to the cfg's decimal places for that step instead, and Y at it is exact. The step to the next
// iterate solves J d = -Y with Y and J to the cfg's step places: J by divided differences at the first iterate, then
// Broyden's update, J += ((dY - J dp) dp^T) / (dp^T dp).
// Checks:
// 1. Every atom the forms hold is one atom_value.h writes in its independent numbers.
// 2. At the last iterate every |Y_i| is no more than the cfg's tolerance.
// 3. Every exact value is held in the build's width.
// 4. Every iterate, its Y and the last iterate's datum forms are written whole to the record the cfg names.
// The request: datum_fixed_point <cfg>.
//     SIM_EXACT_LIMBS=1024 bash examples/navier_stokes/run.sh datum_fixed_point examples/navier_stokes/cfg/datum_fixed_point.cfg

typedef struct
{
    PressureDatumRequest request;
    EtaFunction swirl;
    EtaFunction axial;
    unsigned int order;
    SimRational inner;
    SimRational outer;
    AtomBook book;
    AtomValues numbers;
    // the atoms' values as the book names them, held[i] 1 where atoms[i] is valued
    std::vector<SimRational> atoms;
    std::vector<int> held;
    int named;
    // the last evaluation's datum forms, one a node
    std::vector<AtomForm> forms;
} DatumFixedPointRun;

// sum values[k] T_k(eta)
static SimRational datum_fixed_point_chebyshev(const std::vector<SimRational> &values, SimRational eta)
{
    SimRational before = sim_rational(1ll, 1ll);
    SimRational now = eta;
    SimRational sum = sim_rational(0ll, 1ll);
    for (size_t mode = 0u; mode < values.size(); mode += 1u)
    {
        sum = sim_rational_sum(sum, sim_rational_product(values[mode], (mode == 0u) ? before : now));
        if (mode >= 1u)
        {
            const SimRational next =
                sim_rational_difference(sim_rational_product(sim_rational_product(sim_rational(2ll, 1ll), eta), now), before);
            before = now;
            now = next;
        }
    }
    return sum;
}

// the value to `places` decimals, the nearest, a half taken away from 0
static SimRational datum_fixed_point_decimal(SimRational value, unsigned int places)
{
    SimRational scale;
    sim_exact_power(10ull, places, &scale.numerator);
    scale.denominator = atom_form_unit();
    const SimRational half = sim_rational((sim_rational_sign(value) < 0) ? -1ll : 1ll, 2ll);
    const SimRational shifted = sim_rational_sum(sim_rational_product(value, scale), half);
    SimRational whole;
    AnchorExactInteger remainder;
    if (anchor_exact_divide(&shifted.numerator, &shifted.denominator, &whole.numerator, &remainder) != ANCHOR_EXACT_OK)
    {
        s_sim_rational_wide = 1;
        return sim_rational(0ll, 1ll);
    }
    whole.denominator = atom_form_unit();
    return sim_rational_product(whole, sim_rational_reciprocal(scale));
}

// Y at each eta for Pi_0 = p
static void datum_fixed_point_residual(DatumFixedPointRun *run, const std::vector<SimRational> &p,
                                       const std::vector<SimRational> &etas, std::vector<SimRational> *residual)
{
    static CoreSeries series;
    EtaFunction pressure;
    eta_function_chebyshev(p, &pressure);
    core_series_recursion(&run->request.shape, &run->swirl, &run->axial, &pressure, run->order, &series);
    run->request.series = &series;
    residual->assign(etas.size(), sim_rational(0ll, 1ll));
    run->forms.assign(etas.size(), AtomForm());
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        PressureDatum datum;
        pressure_datum_at(&run->request, etas[index], run->inner, run->outer, &run->book, &datum);
        run->forms[index] = datum.datum;
        while (run->atoms.size() < run->book.names.size())
        {
            run->atoms.push_back(sim_rational(0ll, 1ll));
            run->held.push_back(0);
        }
        for (size_t atom = 0u; atom < run->book.names.size(); atom += 1u)
        {
            if (!run->held[atom])
            {
                run->held[atom] = 1;
                if (!atom_value_named(&run->numbers, run->book.names[atom], &run->atoms[atom]))
                {
                    run->named = 0;
                }
            }
        }
        const SimRational value = atom_value_form(&run->numbers, datum.datum, run->atoms);
        (*residual)[index] = sim_rational_difference(value, datum_fixed_point_chebyshev(p, etas[index]));
    }
}

// the solution of matrix x = right, n by n, by elimination with the first nonzero pivot: 1, or 0 where the matrix is
// singular
static int datum_fixed_point_solve(std::vector<std::vector<SimRational>> matrix, std::vector<SimRational> right,
                                   std::vector<SimRational> *solution)
{
    const size_t size = right.size();
    for (size_t column = 0u; column < size; column += 1u)
    {
        size_t pivot = column;
        while ((pivot < size) && (sim_rational_sign(matrix[pivot][column]) == 0))
        {
            pivot += 1u;
        }
        if (pivot == size)
        {
            return 0;
        }
        std::swap(matrix[pivot], matrix[column]);
        std::swap(right[pivot], right[column]);
        for (size_t row = column + 1u; row < size; row += 1u)
        {
            const SimRational factor = sim_rational_product(matrix[row][column], sim_rational_reciprocal(matrix[column][column]));
            if (sim_rational_sign(factor) == 0)
            {
                continue;
            }
            for (size_t entry = column; entry < size; entry += 1u)
            {
                matrix[row][entry] = sim_rational_difference(matrix[row][entry], sim_rational_product(factor, matrix[column][entry]));
            }
            right[row] = sim_rational_difference(right[row], sim_rational_product(factor, right[column]));
        }
    }
    solution->assign(size, sim_rational(0ll, 1ll));
    for (size_t row = size; row > 0u; row -= 1u)
    {
        SimRational sum = right[row - 1u];
        for (size_t entry = row; entry < size; entry += 1u)
        {
            sum = sim_rational_difference(sum, sim_rational_product(matrix[row - 1u][entry], (*solution)[entry]));
        }
        (*solution)[row - 1u] = sim_rational_product(sum, sim_rational_reciprocal(matrix[row - 1u][row - 1u]));
    }
    return 1;
}

// each entry to `places` decimals
static std::vector<SimRational> datum_fixed_point_decimals(const std::vector<SimRational> &values, unsigned int places)
{
    std::vector<SimRational> decimals;
    for (const SimRational &value : values)
    {
        decimals.push_back(datum_fixed_point_decimal(value, places));
    }
    return decimals;
}

static void datum_fixed_point_list(ScripturaLine *line, const char *name, const std::vector<SimRational> &values,
                                   unsigned int places)
{
    scriptura_text(line, name);
    for (const SimRational &value : values)
    {
        scriptura_character(line, ' ');
        report_value(line, value, places);
    }
    scriptura_character(line, '\n');
}

// 1 where every |values[i]| <= bound
static int datum_fixed_point_within(const std::vector<SimRational> &values, SimRational bound)
{
    for (const SimRational &value : values)
    {
        if (sim_rational_sign(sim_rational_difference(sim_rational_absolute(value), bound)) > 0)
        {
            return 0;
        }
    }
    return 1;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    static RunCfg cfg;
    static DatumFixedPointRun run;
    SimRational h;
    std::vector<SimRational> swirl;
    std::vector<SimRational> axial;
    std::vector<SimRational> start;
    std::vector<SimRational> nodes;
    std::vector<SimRational> between;
    std::vector<unsigned long long> iterate_places;
    SimRational step;
    SimRational tolerance;
    unsigned long long order = 0ull;
    unsigned long long terms = 0ull;
    unsigned long long orders = 0ull;
    unsigned long long length = 0ull;
    unsigned long long step_places = 0ull;
    unsigned long long places = 0ull;
    int read =
        (count == 2) && run_cfg_open(arguments[1], &cfg, &results.line) &&
        run_cfg_rational(&cfg, "core.anisotropy", &h) && (sim_rational_sign(h) > 0) &&
        run_cfg_rationals(&cfg, "core.axis.swirl", &swirl) && run_cfg_rationals(&cfg, "core.axis.axial", &axial) &&
        run_cfg_rationals(&cfg, "core.axis.pressure", &start) && run_cfg_count(&cfg, "core.order", &order) &&
        (order > 0ull) && (order < 4096ull) && run_cfg_rational(&cfg, "join.amplitude", &run.request.amplitude) &&
        run_cfg_rational(&cfg, "join.inner", &run.inner) && run_cfg_rational(&cfg, "join.outer", &run.outer) &&
        (sim_rational_sign(run.inner) > 0) && (sim_rational_sign(sim_rational_difference(run.outer, run.inner)) > 0) &&
        run_cfg_rationals(&cfg, "blend.cuts", &run.request.pieces.cuts) && (run.request.pieces.cuts.size() >= 3u) &&
        run_cfg_count(&cfg, "blend.terms", &terms) && (terms > 2ull) && run_cfg_count(&cfg, "blend.orders", &orders) &&
        (orders > 0ull) && run_cfg_rationals(&cfg, "fixed_point.nodes", &nodes) && !nodes.empty() &&
        (start.size() <= nodes.size()) && run_cfg_rationals(&cfg, "fixed_point.between", &between) &&
        run_cfg_count(&cfg, "fixed_point.length", &length) && (length >= 2ull) && (length <= 256ull) &&
        run_cfg_rational(&cfg, "fixed_point.step", &step) && (sim_rational_sign(step) > 0) &&
        run_cfg_counts(&cfg, "fixed_point.iterate_places", &iterate_places) && !iterate_places.empty() &&
        run_cfg_count(&cfg, "fixed_point.step_places", &step_places) && (step_places <= 4096ull) &&
        run_cfg_rational(&cfg, "fixed_point.tolerance", &tolerance) && (sim_rational_sign(tolerance) > 0) &&
        run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull);
    std::vector<SimRational> etas = nodes;
    etas.insert(etas.end(), between.begin(), between.end());
    for (size_t index = 0u; read && (index < etas.size()); index += 1u)
    {
        read = sim_rational_sign(sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(etas[index], etas[index]))) > 0;
    }
    for (size_t index = 0u; read && (index < iterate_places.size()); index += 1u)
    {
        read = iterate_places[index] <= 4096ull;
    }
    if (!read)
    {
        if (count == 2)
        {
            run_cfg_missing(&results.line, "core anisotropy, axis swirl, axial and pressure (no more modes than nodes), core "
                                           "order, join amplitude, inner and outer, blend cuts, terms above 2 and orders, "
                                           "fixed_point nodes, between, length from 2 to 256, step above 0, iterate_places, "
                                           "step_places and tolerance above 0, every eta inside (-1, 1), report places");
        }
        sim_flush(&results);
        fprintf(stderr, "datum_fixed_point <cfg>\n");
        return 2;
    }
    run.request.h = h;
    run.request.shape.part = h.numerator;
    run.request.shape.whole = h.denominator;
    run.request.pieces.terms = (unsigned int)terms;
    run.request.pieces.orders = (unsigned int)orders;
    run.order = (unsigned int)order;
    run.named = 1;
    eta_function_chebyshev(swirl, &run.swirl);
    eta_function_chebyshev(axial, &run.axial);
    atom_value_open(&run.numbers, h, (unsigned int)length);
    const size_t modes = nodes.size();
    std::vector<SimRational> p = start;
    p.resize(modes, sim_rational(0ll, 1ll));

    FILE *const record = record_open(arguments[1], &cfg, "report.record");
    std::vector<SimRational> residual;
    datum_fixed_point_residual(&run, p, nodes, &residual);
    datum_fixed_point_list(&results.line, "  iterate 0: p", p, (unsigned int)places);
    datum_fixed_point_list(&results.line, "    Y at the nodes", residual, 2u);
    sim_flush(&results);
    // J by divided differences at the first iterate
    std::vector<std::vector<SimRational>> jacobian(modes, std::vector<SimRational>(modes, sim_rational(0ll, 1ll)));
    for (size_t mode = 0u; mode < modes; mode += 1u)
    {
        std::vector<SimRational> moved = p;
        moved[mode] = sim_rational_sum(moved[mode], step);
        std::vector<SimRational> shifted;
        datum_fixed_point_residual(&run, moved, nodes, &shifted);
        for (size_t row = 0u; row < modes; row += 1u)
        {
            jacobian[row][mode] = datum_fixed_point_decimal(
                sim_rational_product(sim_rational_difference(shifted[row], residual[row]), sim_rational_reciprocal(step)),
                (unsigned int)step_places);
        }
    }
    for (size_t iterate = 0u; iterate < iterate_places.size(); iterate += 1u)
    {
        if (record != NULL)
        {
            for (size_t mode = 0u; mode < modes; mode += 1u)
            {
                record_form(record, ("iterate_" + std::to_string(iterate) + "_p_" + std::to_string(mode)).c_str(),
                            atom_form_rational(p[mode]), &run.book);
            }
            for (size_t node = 0u; node < modes; node += 1u)
            {
                record_form(record, ("iterate_" + std::to_string(iterate) + "_Y_" + std::to_string(node)).c_str(),
                            atom_form_rational(residual[node]), &run.book);
            }
        }
        const std::vector<SimRational> right = datum_fixed_point_decimals(residual, (unsigned int)step_places);
        std::vector<SimRational> change;
        if (!datum_fixed_point_solve(jacobian, right, &change))
        {
            scriptura_text(&results.line, "  the step's matrix is singular\n");
            break;
        }
        std::vector<SimRational> next(modes);
        std::vector<SimRational> moved(modes);
        for (size_t mode = 0u; mode < modes; mode += 1u)
        {
            next[mode] = datum_fixed_point_decimal(sim_rational_difference(p[mode], change[mode]),
                                                   (unsigned int)iterate_places[iterate]);
            moved[mode] = sim_rational_difference(next[mode], p[mode]);
        }
        std::vector<SimRational> next_residual;
        datum_fixed_point_residual(&run, next, nodes, &next_residual);
        // Broyden's update on the decimals
        const std::vector<SimRational> rise =
            datum_fixed_point_decimals(next_residual, (unsigned int)step_places);
        SimRational norm = sim_rational(0ll, 1ll);
        for (size_t mode = 0u; mode < modes; mode += 1u)
        {
            norm = sim_rational_sum(norm, sim_rational_product(moved[mode], moved[mode]));
        }
        if (sim_rational_sign(norm) != 0)
        {
            for (size_t row = 0u; row < modes; row += 1u)
            {
                SimRational miss = sim_rational_difference(rise[row], right[row]);
                for (size_t mode = 0u; mode < modes; mode += 1u)
                {
                    miss = sim_rational_difference(miss, sim_rational_product(jacobian[row][mode], moved[mode]));
                }
                for (size_t mode = 0u; mode < modes; mode += 1u)
                {
                    jacobian[row][mode] = datum_fixed_point_decimal(
                        sim_rational_sum(jacobian[row][mode],
                                         sim_rational_product(miss, sim_rational_product(moved[mode], sim_rational_reciprocal(norm)))),
                        (unsigned int)step_places);
                }
            }
        }
        p = next;
        residual = next_residual;
        scriptura_text(&results.line, "  iterate ");
        scriptura_decimal(&results.line, iterate + 1u, 1u);
        scriptura_text(&results.line, " to ");
        scriptura_decimal(&results.line, iterate_places[iterate], 1u);
        scriptura_text(&results.line, " places:");
        datum_fixed_point_list(&results.line, " p", p, (unsigned int)places);
        datum_fixed_point_list(&results.line, "    Y at the nodes", residual, 2u);
        sim_flush(&results);
    }
    // the last iterate between the nodes
    std::vector<SimRational> last_between;
    if (!between.empty())
    {
        datum_fixed_point_residual(&run, p, between, &last_between);
        datum_fixed_point_list(&results.line, "  Y between the nodes", last_between, 2u);
    }
    datum_fixed_point_residual(&run, p, nodes, &residual);
    if (record != NULL)
    {
        const std::string at = "iterate_" + std::to_string(iterate_places.size());
        for (size_t mode = 0u; mode < modes; mode += 1u)
        {
            record_form(record, (at + "_p_" + std::to_string(mode)).c_str(), atom_form_rational(p[mode]), &run.book);
        }
        for (size_t node = 0u; node < modes; node += 1u)
        {
            record_form(record, (at + "_Y_" + std::to_string(node)).c_str(), atom_form_rational(residual[node]), &run.book);
            record_form(record, ("datum_eta_" + atom_book_rational(nodes[node])).c_str(), run.forms[node], &run.book);
        }
        for (size_t index = 0u; index < last_between.size(); index += 1u)
        {
            record_form(record, ("between_Y_eta_" + atom_book_rational(between[index])).c_str(),
                        atom_form_rational(last_between[index]), &run.book);
        }
    }

    // 1. the atoms
    scriptura_text(&results.line, run.named ? "  every atom is written in its independent numbers\n"
                                            : "  an atom has no value\n");
    sim_check(&results, run.named, "every atom valued");
    // 2. the residual at the last iterate
    const int settled = datum_fixed_point_within(residual, tolerance);
    scriptura_text(&results.line, settled ? "  at the last iterate every |Y| at the nodes is within the cfg's tolerance\n"
                                          : "  at the last iterate a |Y| at a node is past the cfg's tolerance\n");
    sim_check(&results, settled, "the datum returns Pi_0");
    // 4. the record, before the width
    const int recorded = (record != NULL) && record_close(record);
    // 3. the width
    const int wide[13] = {s_sim_rational_wide, run_cfg_short(),     report_short(),      atom_form_short(),
                          taylor_short(),      ode_series_short(),  eta_function_short(), core_series_short(),
                          decay_integral_short(), blend_short(),    pressure_datum_short(), atom_value_short(),
                          record_short()};
    static const char *const modules[13] = {"datum_fixed_point", "run_cfg", "report", "atom_form", "taylor", "ode_series",
                                            "eta_function", "core_series", "decay_integral", "blend", "pressure_datum",
                                            "atom_value", "record"};
    int held = 1;
    for (size_t module = 0u; module < 13u; module += 1u)
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
    scriptura_text(&results.line, recorded ? "  every iterate and its Y is written whole to the cfg's record\n"
                                           : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(&results, recorded, "record written");
    return sim_close(&results, "datum fixed point");
}
